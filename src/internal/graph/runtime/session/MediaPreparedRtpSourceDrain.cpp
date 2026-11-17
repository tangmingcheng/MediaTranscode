#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::ErrorInfo invalidClosing(const char* text)
{ return ::media::ErrorInfo::invalidArgument(text); }
}

const MediaAvSourceDrainMember& MediaPreparedRtpSourceSession::closingMember(const Stream& s) const
{
    return s.plan.depacketizer.streamKind == MediaStreamKind::Video ? m_closing->request.video
        : *m_closing->request.audio;
}

bool MediaPreparedRtpSourceSession::controlFits(const Stream& s,
    const MediaRtpPreparedClockObservation& prepared) const
{
    if (!m_frozen || std::holds_alternative<std::monostate>(prepared.tracker.output)) return true;
    if (!s.plan.runtimeCredit) return false;
    const auto& credit = *s.plan.runtimeCredit;
    const auto* evidence = std::get_if<MediaRtcpClockEvidence>(&prepared.tracker.output);
    const std::size_t bytes = sizeof(MediaPreparedRtpOutput) + sizeof(MediaRtpIngressEventBuffer) +
        (evidence ? evidence->cname.size() : 0);
    return s.controls.size() < credit.maximumControlObjects && bytes <= credit.maximumControlBytes &&
        s.controlBytes <= credit.maximumControlBytes - bytes;
}

bool MediaPreparedRtpSourceSession::generationBlocked(const Stream& s,
    MediaRtpUdpChannel channel, std::int64_t observedAtNs) const
{
    if (!m_closing) return false;
    if (m_closing->opening || m_closing->cancelled || s.closing.frontier || s.closing.purgeSequence)
        return true;
    // Neither RTP nor fresh SR evidence may enter the old member after its
    // immutable capture cutoff. The original receiver batch retains the entry.
    (void)channel;
    return observedAtNs >= closingMember(s).closeDeadlineNs;
}

::media::Result<std::optional<MediaPreparedRtpIngressProgress>>
MediaPreparedRtpSourceSession::consumeDeparture(MediaStreamKind kind,
    std::span<const std::uint8_t> bytes, std::int64_t observedAtNs, std::uint64_t ordinal)
{
    using R = ::media::Result<std::optional<MediaPreparedRtpIngressProgress>>;
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
    std::optional<MediaRtpSenderDeparture> departure;
    {
        std::scoped_lock lock(m_mutex);
        if (m_error) return R::failure(*m_error);
        if (!m_frozen || !m_runtimeGroup || m_closing) return R::success(std::nullopt);
        auto selected = stream(kind);
        if (!selected) return R::failure(selected.error());
        auto prepared = selected.value()->protocol->prepareRtcp(bytes, observedAtNs,
            mediaSteadyClockNowNs(), MediaRtpByeHandling::PreserveMemberForDrain);
        if (!prepared) return R::failure(prepared.error());
        // Preserve the original rejection/strong transition path, before BYE.
        if (prepared.value().tracker.rejection || prepared.value().tracker.invalidated ||
            prepared.value().retireAssembly) return R::success(std::nullopt);
        departure = prepared.value().tracker.senderDeparture;
        if (!departure) return R::success(std::nullopt);
        group = m_runtimeGroup->lock();
    } // Borrowed prepared state dies before arbitration or another source lock.
    if (!group) return R::failure(invalidClosing("Prepared RTP source lost its runtime group"));
    auto authority = group->reserveGenerationArbitration();
    if (!authority) return R::failure(authority.error());
    const auto clock = group->clockEvidence();
    const auto* epoch = authority.value().epoch().playbackEpoch();
    // Only a never-published source uses startup BYE semantics.
    if (!epoch) return R::success(std::nullopt);
    auto disposition = classifyMediaAvGenerationEvidence(authority.value().reacquisition(),
        authority.value().epoch(), epoch->generation);
    if (!disposition) return R::failure(disposition.error());
    std::unique_lock lock(m_mutex);
    if (m_error) return R::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return R::failure(selected.error());
    auto& s = *selected.value();
    if (m_closing) return R::success(std::nullopt);
    if (disposition.value() != MediaAvGenerationEvidenceDisposition::Target ||
        !authority.value().epoch().outputPermitted || !m_runtimeClockLocked || !clock ||
        clock->generation != epoch->generation || m_runtimeClockGeneration != epoch->generation ||
        !s.runtimeIngressGeneration || !s.runtimeSsrc ||
        s.runtimeIngressGeneration != departure->ingressGeneration || s.runtimeSsrc != departure->ssrc)
        return R::failure(invalidClosing("Published RTP departure does not match its activated clock/member identity"));
    if (auto status = validateIngressLocked(s, bytes, observedAtNs, ordinal); !status)
        return R::failure(status.error());
    MediaAvSourceDrainRequest request{epoch->generation, {}, std::nullopt};
    {
        auto prepared = s.protocol->prepareRtcp(bytes, observedAtNs, mediaSteadyClockNowNs(),
            MediaRtpByeHandling::PreserveMemberForDrain);
        if (!prepared) return R::failure(prepared.error());
        const auto& confirmed = prepared.value().tracker.senderDeparture;
        if (prepared.value().tracker.rejection || prepared.value().tracker.invalidated || prepared.value().retireAssembly)
            return R::success(std::nullopt);
        if (!confirmed || confirmed->ingressGeneration != departure->ingressGeneration ||
            confirmed->ssrc != departure->ssrc || confirmed->observedAtNs != departure->observedAtNs)
            return R::failure(invalidClosing("RTP departure changed while obtaining publication authority"));
        if (!controlFits(s, prepared.value())) {
            s.waitingCredit = MediaPreparedRtpCreditBlocker::ControlObjects;
            return R::success(MediaPreparedRtpIngressProgress{MediaPreparedRtpIngressState::NotAcceptedWaitingControl,
                s.waitingCredit});
        }
        const auto member = [&](const Stream& item) -> ::media::Result<MediaAvSourceDrainMember> {
            const auto delay = item.plan.reorder.maximumDelay.count();
            if (!item.runtimeIngressGeneration || !item.runtimeSsrc || delay <= 0 ||
                observedAtNs > (std::numeric_limits<std::int64_t>::max)() - delay ||
                item.protocol->generation() != *item.runtimeIngressGeneration)
                return ::media::Result<MediaAvSourceDrainMember>::failure(invalidClosing(
                    "RTP closing lacks the exact member or representable prepared input wait"));
            return ::media::Result<MediaAvSourceDrainMember>::success({*item.runtimeIngressGeneration,
                *item.runtimeSsrc, &item == &s ? MediaAvSourceCloseReason::SenderLeft : MediaAvSourceCloseReason::GroupClose,
                observedAtNs, observedAtNs + delay});
        };
        auto video = member(*m_video);
        if (!video) return R::failure(video.error());
        request.video = video.value();
        if (m_audio) {
            auto audio = member(*m_audio);
            if (!audio) return R::failure(audio.error());
            request.audio = audio.value();
        }
        s.waitingCredit.reset();
        if (auto status = commitControl(s, std::move(prepared).value(), mediaSteadyClockNowNs(), nullptr); !status)
            return R::failure(status.error());
    } // No borrowed protocol delta crosses the publication call.
    m_closing.emplace(Closing{request});
    m_video->closing = {};
    if (m_audio) m_audio->closing = {};
    s.ordinal = ordinal;
    lock.unlock();
    const auto begun = group->beginSourceDrain(request);
    lock.lock();
    if (!begun && !m_error) m_error = begun.error();
    if (!m_closing || m_closing->request.playbackGeneration != request.playbackGeneration) {
        if (!m_error) m_error = invalidClosing("RTP Opening identity changed during publication");
    } else m_closing->opening = false;
    for (auto* item : {m_video.get(), m_audio.get()}) {
        if (!item) continue;
        notifyCredit(*item);
        if (item->wakeup) item->wakeup->notify();
        if (item->controlWakeup) item->controlWakeup->notify();
    }
    m_ready.notify_all();
    if (m_error) return R::failure(*m_error);
    if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State))
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
            "prepared_rtp_source_closing generation=" + std::to_string(request.playbackGeneration) +
            " video_deadline_ns=" + std::to_string(request.video.closeDeadlineNs) +
            " audio_deadline_ns=" + (request.audio ? std::to_string(request.audio->closeDeadlineNs) : "none"));
    return R::success(MediaPreparedRtpIngressProgress{MediaPreparedRtpIngressState::AcceptedReady, std::nullopt});
}

::media::Status MediaPreparedRtpSourceSession::progressClosing(MediaStreamKind kind, std::int64_t now)
{
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
    {
        std::scoped_lock lock(m_mutex);
        if (m_error) return ::media::Status::failure(*m_error);
        if (!m_closing || m_closing->opening) return ::media::Status::success();
        group = m_runtimeGroup ? m_runtimeGroup->lock() : nullptr;
    }
    if (!group) return ::media::Status::failure(invalidClosing("Closing RTP source lost its runtime group"));
    auto authority = group->reserveGenerationArbitration();
    if (!authority) return ::media::Status::failure(authority.error());
    std::unique_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (!m_closing || m_closing->opening) return ::media::Status::success();
    const auto generation = m_closing->request.playbackGeneration;
    auto disposition = classifyMediaAvGenerationEvidence(authority.value().reacquisition(), authority.value().epoch(), generation);
    if (!disposition) return ::media::Status::failure(disposition.error());
    const auto& reacquisition = authority.value().reacquisition();
    if (disposition.value() != MediaAvGenerationEvidenceDisposition::Target || !authority.value().epoch().outputPermitted) {
        if (reacquisition.reason != MediaAvReacquisitionReason::SourceDrained)
            m_closing->cancelled = true;
        if (!m_closing->cancelled) {
            // The Raw media purge authorizes retirement independently of its
            // queue credit. It must still belong to this exact source transition.
            for (const auto* member : {m_video.get(), m_audio.get()}) {
                if (!member || !member->closing.purgeSequence) continue;
                if (!member->lastPurge || !reacquisition.transition ||
                    member->lastPurge->transitionSequence != reacquisition.transition->transitionSequence ||
                    member->lastPurge->oldGeneration != generation ||
                    member->lastPurge->oldGeneration != reacquisition.transition->oldGeneration ||
                    member->lastPurge->nextGeneration != reacquisition.transition->nextGeneration ||
                    member->lastPurge->publishedGeneration != reacquisition.transition->publishedGeneration)
                    return ::media::Status::failure(invalidClosing("RTP retirement lost its frozen purge authorization"));
            }
            if (auto status = progressRetirementsLocked(now); !status) return status;
        }
        finishRetirementsLocked();
        return ::media::Status::success();
    }
    if (m_closing->cancelled) {
        finishRetirementsLocked();
        return ::media::Status::success();
    }
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    auto& s = *selected.value();
    if (s.closing.endQueued) return ::media::Status::success();
    const auto member = closingMember(s);
    if (!s.rtpCaptureExclusiveBeforeNs || *s.rtpCaptureExclusiveBeforeNs < member.closeDeadlineNs)
        return ::media::Status::success();
    if (auto status = tickLocked(s, now); !status) return status;
    if (m_closing->cancelled || s.waitingCredit) return ::media::Status::success();
    const auto tail = s.protocol->drainSnapshot();
    if (tail.reorderedPackets || tail.pendingItems) return ::media::Status::success();
    if (s.protocol->generation() != member.ingressGeneration)
        return ::media::Status::failure(invalidClosing("Closing RTP protocol changed before input seal"));
    const auto& credit = *s.plan.runtimeCredit;
    constexpr auto bytes = sizeof(MediaPreparedRtpOutput) + sizeof(MediaControlBuffer);
    if (s.packets.size() >= credit.maximumPacketObjects || bytes > credit.maximumPacketBytes ||
        s.packetBytes > credit.maximumPacketBytes - bytes) {
        s.waitingCredit = MediaPreparedRtpCreditBlocker::PacketObjects;
        return ::media::Status::success();
    }
    // This enum is absence of complete AU proof, not a claim of a broken FU.
    if (tail.assembly == MediaRtpAssemblyState::Incomplete) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
            "prepared_rtp_terminal_incomplete stream=" + std::to_string(static_cast<int>(kind)) +
            " generation=" + std::to_string(generation) + " ingress_generation=" + std::to_string(member.ingressGeneration));
        s.protocol->resetAssembly();
        s.pending.clear();
    }
    auto marker = std::allocate_shared<MediaControlBuffer>(
        std::pmr::polymorphic_allocator<MediaControlBuffer>{s.resource.get()}, MediaControlBufferKind::EndGeneration, generation);
    // Freeze before releasing source lock; no new entry may cross this fence.
    s.closing.frontier = s.nextSequence - 1;
    const auto frontier = *s.closing.frontier;
    lock.unlock();
    auto sealed = group->sealSourceDrainInput(generation, kind, member.ingressGeneration, frontier);
    lock.lock();
    if (!sealed) return sealed;
    if (m_error) return ::media::Status::failure(*m_error);
    if (!m_closing || m_closing->cancelled || m_closing->request.playbackGeneration != generation)
        return ::media::Status::success();
    if (auto status = queue(s, MediaPreparedRtpOutputPort::Packet, std::move(marker)); !status) return status;
    s.closing.endQueued = true;
    if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State))
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
            "prepared_rtp_input_sealed stream=" + std::to_string(static_cast<int>(kind)) +
            " generation=" + std::to_string(generation) + " ingress_generation=" + std::to_string(member.ingressGeneration) +
            " frontier=" + std::to_string(frontier));
    if (s.wakeup) s.wakeup->notify();
    notifyCredit(s);
    return ::media::Status::success();
}

bool MediaPreparedRtpSourceSession::finishRetirementsLocked()
{
    if (!m_closing) return true;
    for (const auto* s : {m_video.get(), m_audio.get()}) {
        if (!s) continue;
        if (!s->closing.purgeSequence) return false;
        if (m_closing->cancelled && s->lastControlSequence &&
            (!s->consumedControlSequence || *s->consumedControlSequence < *s->lastControlSequence))
            return false;
        // Cancellation can waive an unproduced retirement. A queued old
        // invalidation remains capable of clearing both new member mappings.
        if (m_closing->cancelled && !s->closing.retirementSequence) continue;
        const auto& retirement = s->closing.retirement;
        const auto& consumed = s->consumedRetirement;
        if (!retirement || !consumed || s->closing.retirementSequence != s->consumedRetirementSequence ||
            retirement->generation != consumed->generation || retirement->reason != consumed->reason ||
            retirement->observedAtNs != consumed->observedAtNs) return false;
    }
    if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State))
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
            "prepared_rtp_generation_released generation=" + std::to_string(m_closing->request.playbackGeneration) +
            " cancelled=" + std::to_string(m_closing->cancelled));
    m_closing.reset();
    for (auto* s : {m_video.get(), m_audio.get()}) {
        if (!s) continue;
        s->closing = {};
        notifyCredit(*s);
        if (s->wakeup) s->wakeup->notify();
    }
    return true;
}
} // namespace media::ffmpeg::graph
