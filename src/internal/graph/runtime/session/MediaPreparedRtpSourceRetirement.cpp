#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
::media::Result<std::optional<std::uint64_t>> MediaPreparedRtpSourceSession::purgeMedia(
    MediaStreamKind kind, const MediaAvGenerationPurge& purge)
{
    using R = ::media::Result<std::optional<std::uint64_t>>;
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
    {
        std::scoped_lock lock(m_mutex);
        if (m_error) return R::failure(*m_error);
        group = m_runtimeGroup ? m_runtimeGroup->lock() : nullptr;
    }
    if (!group) return R::failure(::media::ErrorInfo::notInitialized("Prepared RTP purge lacks its runtime group"));
    auto authority = group->reserveGenerationArbitration();
    if (!authority) return R::failure(authority.error());
    const auto& transition = authority.value().reacquisition().transition;
    if (!transition || transition->transitionSequence != purge.transitionSequence ||
        transition->oldGeneration != purge.oldGeneration || transition->nextGeneration != purge.nextGeneration ||
        transition->publishedGeneration != purge.publishedGeneration)
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP purge has a stale transition identity"));
    std::scoped_lock lock(m_mutex);
    if (m_error) return R::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return R::failure(selected.error());
    if (!m_frozen) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP purge precedes handoff"));
    auto& s = *selected.value();
    if (s.lastPurge && s.lastPurge->transitionSequence > purge.transitionSequence)
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP purge transition regressed"));
    if (s.lastPurge && s.lastPurge->transitionSequence == purge.transitionSequence) {
        if (s.lastPurge->oldGeneration != purge.oldGeneration || s.lastPurge->nextGeneration != purge.nextGeneration ||
            s.lastPurge->publishedGeneration != purge.publishedGeneration || !s.purgeFrontier)
            return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP repeated purge changed identity"));
        return R::success(s.purgeFrontier);
    }
    if (m_closing) {
        if (m_closing->request.playbackGeneration != purge.oldGeneration)
            return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP purge does not own the closing generation"));
        if (authority.value().reacquisition().reason != MediaAvReacquisitionReason::SourceDrained)
            m_closing->cancelled = true;
        if (!m_closing->cancelled) {
            if (!s.closing.endQueued || !s.closing.frontier)
                return R::failure(::media::ErrorInfo::invalidArgument("Normal RTP retirement precedes the ordered input end"));
        }
        s.closing.purgeSequence = purge.transitionSequence;
    }
    std::erase_if(s.packets, [&](const auto& item) {
        if (item.port != MediaPreparedRtpOutputPort::Packet) return false;
        s.outputBytes -= item.admissionBytes;
        s.packetBytes -= item.admissionBytes;
        return true;
    });
    s.protocol->resetAssembly();
    s.pending.clear();
    s.protocol->requireKeyframe();
    s.lastPurge = purge;
    s.purgeFrontier = s.nextSequence - 1;
    notifyCredit(s);
    finishRetirementsLocked();
    // Neither retirement queue credit nor its consumption may delay this media
    // frontier: downstream clock consumers need it to release backpressure.
    // Receiver tick/credit wakeups own subsequent retirement production.
    return R::success(s.purgeFrontier);
}
::media::Status MediaPreparedRtpSourceSession::progressRetirementsLocked(std::int64_t now)
{
    if (!m_closing || m_closing->cancelled) return ::media::Status::success();
    for (auto* selected : {m_video.get(), m_audio.get()}) {
        if (!selected) continue;
        auto& s = *selected;
        if (!s.closing.purgeSequence || s.closing.retirementSequence) continue;
        const auto& member = closingMember(s);
        auto prepared = s.protocol->prepareMemberRetirement(member.ingressGeneration, member.ssrc,
            member.reason == MediaAvSourceCloseReason::SenderLeft ? MediaRtpSourceUnavailableReason::SenderLeft
                : MediaRtpSourceUnavailableReason::GroupClosed, member.observedAtNs);
        if (!prepared) return ::media::Status::failure(prepared.error());
        if (!controlFits(s, prepared.value())) {
            s.waitingCredit = MediaPreparedRtpCreditBlocker::ControlObjects;
            continue;
        }
        const auto retirement = std::get<MediaRtpClockInvalidation>(prepared.value().tracker.output);
        const auto sequence = s.nextSequence;
        const auto committed = [&]() {
            struct RetirementCommit final {
                bool& active;
                ~RetirementCommit() { active = false; }
            } scope{m_committingRetirement};
            m_committingRetirement = true;
            return commitControl(s, std::move(prepared).value(), now, nullptr);
        }();
        if (!committed) return committed;
        s.closing.retirementSequence = sequence;
        s.closing.retirement = retirement;
        if (s.controlWakeup) s.controlWakeup->notify();
        m_ready.notify_all();
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
