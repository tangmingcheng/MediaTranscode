#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(message));
}


void logAcquisitionBoundary(
    const char* stage,
    MediaNodeId node,
    const MediaAvSyncGroupRuntime& group,
    const MediaBufferRef& buffer,
    std::size_t buffered,
    const std::optional<MediaRtpClockGroupSnapshot>& locked)
{
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    const auto transition = group.reacquisitionSnapshot();
    std::string message = std::string("rtp_binder_boundary stage=") + stage +
        " node=" + std::to_string(node.value) + " group=" + group.key().value() +
        " buffered=" + std::to_string(buffered) +
        " phase=" + std::to_string(static_cast<int>(transition.phase)) +
        " preserves_output=" + (group.preservesActivatedOutput() ? "1" : "0");
    if (packet && packet->rtpProvenance()) {
        const auto& raw = *packet->rtpProvenance();
        message += " raw_ssrc=" + std::to_string(raw.ssrc) +
            " raw_generation=" + std::to_string(raw.ingressGeneration) +
            " raw_sequence=" + std::to_string(raw.ingressSequence);
    }
    if (transition.transition) {
        const auto& purge = *transition.transition;
        message += " old_generation=" + std::to_string(purge.oldGeneration) +
            " next_generation=" + std::to_string(purge.nextGeneration) +
            " transition_sequence=" + std::to_string(purge.transitionSequence) +
            " published_generation=" + std::to_string(purge.publishedGeneration);
    }
    if (locked && locked->locked) {
        message += " protocol_generation=" + std::to_string(locked->groupGeneration) +
            " evidence_revision=" + std::to_string(locked->evidenceRevision) +
            " video_raw_generation=" + std::to_string(locked->locked->video.generation) +
            " video_ssrc=" + std::to_string(locked->locked->video.ssrc);
        if (locked->locked->audio) {
            message += " audio_raw_generation=" +
                std::to_string(locked->locked->audio->generation) +
                " audio_ssrc=" + std::to_string(locked->locked->audio->ssrc);
        }
    }
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode, message);
}

} // namespace

MediaRtpPacketClockBinderNode::~MediaRtpPacketClockBinderNode() = default;

MediaRtpPacketClockBinderNode::MediaRtpPacketClockBinderNode(
    MediaNodeId nodeId, MediaStreamKind stream)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpPacketClockBinderNode")
    , m_ownerPurge(std::make_shared<MediaOwnerThreadGenerationPurge>())
    , m_streamKind(stream)
{
}

MediaNodeKind MediaRtpPacketClockBinderNode::staticKind() noexcept
{
    return MediaNodeKind::RtpPacketClockBinder;
}

::media::Status MediaRtpPacketClockBinderNode::bindPreparedSession(
    std::shared_ptr<MediaPreparedRtpSourceSession> source)
{
    if (!source || m_preparedSource || m_configured)
        return invalid("RTP binder requires one prepared source before configuration");
    m_preparedSource = std::move(source);
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::start(MediaGraphExecutionContext& context)
{
    auto status = FFmpegNodeRuntime::start(context);
    if (status) status = configure(context);
    if (status) status = m_ownerPurge->start(context.sharedNodeWakeup(nodeId()));
    if (!status) {
        resetState();
        FFmpegNodeRuntime::abort(context);
    }
    return status;
}

std::string_view MediaRtpPacketClockBinderNode::generationPurgeIdentity() const noexcept
{
    return m_streamKind == MediaStreamKind::Video
        ? MediaRtpVideoBinderGenerationIdentity : MediaRtpAudioBinderGenerationIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaRtpPacketClockBinderNode::generationPurgeTarget() const noexcept
{
    return m_ownerPurge;
}

::media::Result<MediaNodeProcessResult> MediaRtpPacketClockBinderNode::process(
    MediaGraphExecutionContext& context)
{
    if (const auto purge = m_ownerPurge->pending()) {
        if (m_preparedSource) {
            auto ledger = context.payloadCreditLedger();
            auto cancelled = ledger ? ledger->cancelProducerWaiter(
                {nodeId(), m_streamKind, MediaPayloadKind::Packet})
                : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "Prepared RTP binder purge requires its payload credit ledger"));
            if (!cancelled) {
                const auto completed = m_ownerPurge->complete(*purge, cancelled);
                return processProgress(completed ? cancelled : completed);
            }
        }
        cancelPendingOutputTransfer();
        if (m_preparedPrefix) m_preparedPrefix->packets.clear();
        m_preparedPendingCredit.reset();
        m_clockSession->invalidateClockProjection();
        m_clockSession->m_acquiringPackets->clear();
        m_clockSession->m_acquisitionDeadline->clear();
        m_clockSession->m_discardingUnboundCandidates = false;
        auto frontier = m_syncGroup->rtpIngressFrontiers()->find(m_streamKind, *purge);
        if (frontier && !frontier.value()) return processWaiting();
        const auto status = frontier ? ::media::Status::success()
            : ::media::Status::failure(frontier.error());
        if (frontier) {
            m_clockSession->m_ingressCutoff = *frontier.value();
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "rtp_binder_purge node=" + std::to_string(nodeId().value) +
                " transition=" + std::to_string(purge->transitionSequence) +
                " cutoff=" + std::to_string(*m_clockSession->m_ingressCutoff));
        }
        const auto completed = m_ownerPurge->complete(*purge, status);
        return processProgress(completed ? status : completed);
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<bool> MediaRtpPacketClockBinderNode::retireExpiredCandidates()
{
    using Result = ::media::Result<bool>;
    if (!m_syncGroup->preservesActivatedOutput() || m_clockSession->m_videoLookahead ||
        m_clockSession->m_pendingTerminal || pendingOutputBuffer())
        return Result::success(false);
    auto arbitration = m_syncGroup->reserveGenerationArbitration();
    if (!arbitration) return Result::failure(arbitration.error());
    const auto& source = arbitration.value().reacquisition();
    const auto& epoch = arbitration.value().epoch();
    if (source.phase != MediaAvReacquisitionPhase::Acquiring ||
        !source.transition || epoch.poisoned || epoch.outputPermitted ||
        epoch.readiness != MediaAvGenerationReadiness::Acquiring)
        return Result::success(false);
    for (const auto& packet : *m_clockSession->m_acquiringPackets) {
        auto disposition = m_clockSession->classifyPacket(packet);
        if (!disposition) return Result::failure(disposition.error());
        if (disposition.value() == PacketDisposition::Bind)
            return Result::success(false);
    }
    // These complete access units have never reached the decoder. The existing
    // startup window still enforces random access before any recovered decode.
    m_clockSession->m_acquiringPackets->clear();
    m_clockSession->m_acquisitionDeadline->clear();
    m_clockSession->m_discardingUnboundCandidates = true;
    return Result::success(true);
}

::media::Result<bool> MediaRtpPacketClockBinderNode::resumeCandidateBinding()
{
    using Result = ::media::Result<bool>;
    if (!m_clockSession->m_lockedSnapshot) return Result::failure(
        ::media::ErrorInfo::internalError("Candidate binding requires its classified clock"));
    auto arbitration = m_syncGroup->reserveGenerationArbitration();
    if (!arbitration) return Result::failure(arbitration.error());
    auto disposition = classifyMediaAvGenerationEvidence(
        arbitration.value().reacquisition(), arbitration.value().epoch(),
        m_clockSession->m_lockedSnapshot->groupGeneration);
    if (!disposition) return Result::failure(disposition.error());
    if (disposition.value() != MediaAvGenerationEvidenceDisposition::Target)
        return Result::success(false);
    m_clockSession->m_discardingUnboundCandidates = false;
    return Result::success(true);
}

::media::Result<MediaNodeProcessResult> MediaRtpPacketClockBinderNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (m_preparedPendingCredit) return processProgress(emitPacket(context, m_preparedPendingCredit));
    if (m_preparedPrefix && !m_preparedPrefix->packets.empty()) {
        auto packet = std::move(m_preparedPrefix->packets.front());
        m_preparedPrefix->packets.pop_front();
        return processProgress(emitPacket(context, std::move(packet)));
    }
    if (m_clockSession->m_acquisitionDeadline->deadline()) {
        auto now = m_syncGroup->clock()->now();
        if (!now) {
            return ::media::Result<MediaNodeProcessResult>::failure(now.error());
        }
        if (auto status = m_clockSession->m_acquisitionDeadline->preflight(now.value()); !status) {
            const auto retiredCount = m_clockSession->m_acquiringPackets->size();
            auto retired = retireExpiredCandidates();
            if (!retired) return processProgress(::media::Status::failure(retired.error()));
            if (retired.value()) {
                logAcquisitionBoundary("candidates_retired", nodeId(), *m_syncGroup,
                    {}, retiredCount, m_clockSession->m_lockedSnapshot);
                return processProgress();
            }
            logAcquisitionBoundary("expired", nodeId(), *m_syncGroup,
                m_clockSession->m_acquiringPackets->empty() ? MediaBufferRef{} : m_clockSession->m_acquiringPackets->front(),
                m_clockSession->m_acquiringPackets->size(), m_clockSession->m_lockedSnapshot);
            return processProgress(status);
        }
    }

    if (m_clockSession->m_pendingTerminal) {
        MediaBufferRef terminal = std::move(m_clockSession->m_pendingTerminal);
        return terminal->isEof()
            ? processFinished(emitOutput(context, "packet", terminal))
            : processProgress(emitOutput(context, "packet", terminal));
    }

    auto clock = tryPopInputOptional(context, "clock");
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (clock.value()) {
        if (auto status = acceptClock(*clock.value()); !status) return processProgress(status);
    }

    if (!m_clockSession->m_acquiringPackets->empty()) {
        auto disposition = m_clockSession->classifyPacket(m_clockSession->m_acquiringPackets->front());
        if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
        if (disposition.value() != PacketDisposition::AwaitClock) {
            MediaBufferRef buffered = std::move(m_clockSession->m_acquiringPackets->front());
            m_clockSession->m_acquiringPackets->pop_front();
            if (m_clockSession->m_acquiringPackets->empty()) m_clockSession->m_acquisitionDeadline->clear();
            return disposition.value() == PacketDisposition::Retired ? processProgress()
                : processProgress(bindPacket(context, std::move(buffered)));
        }
    }

    // Keep the next packet in the bounded input channel until clock
    // acquisition can drain our queue. Clock input remains serviced above.
    if (m_clockSession->m_acquiringPackets->size() >= m_clockSession->m_acquiringCapacity) {
        if (!m_clockSession->m_acquisitionDeadline->deadline()) {
            return processProgress(invalid(
                "Full RTP acquisition queue requires an established deadline"));
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntilInputOrDeadline(
                *m_syncGroupKey, *m_clockSession->m_acquisitionDeadline->deadline()));
    }

    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
    }
    if (!packet.value()) {
        if (m_clockSession->m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntilInputOrDeadline(
                    *m_syncGroupKey, *m_clockSession->m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
    MediaBufferRef input = std::move(*packet.value());
    if (input->isEof() || input->isFlush()) {
        return processProgress(finishPacketInput(context, std::move(input)));
    }
    auto disposition = m_clockSession->classifyPacket(input);
    if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
    if (disposition.value() == PacketDisposition::Retired) return processProgress();
    if (m_clockSession->m_discardingUnboundCandidates) {
        if (disposition.value() == PacketDisposition::AwaitClock) return processProgress();
        auto resumed = resumeCandidateBinding();
        if (!resumed) return processProgress(::media::Status::failure(resumed.error()));
        if (!resumed.value()) return processProgress();
        logAcquisitionBoundary("candidate_binding_resumed", nodeId(), *m_syncGroup,
            input, 0, m_clockSession->m_lockedSnapshot);
    }
    if (disposition.value() == PacketDisposition::AwaitClock || !m_clockSession->m_acquiringPackets->empty())
        return processProgress(bufferAcquiring(std::move(input)));
    return processProgress(bindPacket(context, std::move(input)));
}

::media::Status MediaRtpPacketClockBinderNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_configured) return ::media::Status::success();
    const MediaNodeOptions* options = nodeOptions(context);
    auto stream = requiredStreamKindNodeOption(
        options, "MediaRtpPacketClockBinderNode", "rtp_clock_binder.stream");
    auto capacity = requiredPositiveIntNodeOption(
        options, "MediaRtpPacketClockBinderNode",
        "rtp_clock_binder.acquiring_capacity");
    auto timeout = requiredPositiveInt64NodeOption(
        options, "MediaRtpPacketClockBinderNode",
        "rtp_clock_binder.acquiring_timeout_ns");
    auto groupName = requiredNodeOption(
        options, "MediaRtpPacketClockBinderNode",
        "rtp_clock_binder.sync_group");
    if (!stream) return ::media::Status::failure(stream.error());
    if (!capacity) return ::media::Status::failure(capacity.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    if (!groupName) return ::media::Status::failure(groupName.error());
    if (stream.value() != m_streamKind)
        return invalid("RTP binder stream differs from its registration identity");
    std::optional<int> videoClockRate;
    std::optional<MediaRtpVideoTimingPlan> videoTiming;
    m_syncGroupKey.emplace(std::move(groupName).value());
    m_syncGroup = context.findAvSyncGroup(*m_syncGroupKey);
    if (!m_syncGroup || !m_syncGroup->clock() || !m_syncGroup->rtpIngressFrontiers()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP packet binder requires a registered sync-group master clock"));
    }
    if (m_streamKind == MediaStreamKind::Video) {
        auto clockRate = requiredPositiveIntNodeOption(
            options, "MediaRtpPacketClockBinderNode",
            "rtp_clock_binder.video_clock_rate");
        auto product = requiredNodeOption(
            options, "MediaRtpPacketClockBinderNode",
            "rtp_clock_binder.video_timing_product");
        if (!clockRate) return ::media::Status::failure(clockRate.error());
        if (!product) return ::media::Status::failure(product.error());
        if (product.value() == "canonical_interval") {
            auto terminalPolicy = requiredNodeOption(options, "MediaRtpPacketClockBinderNode",
                "rtp_clock_binder.terminal_duration_policy");
            if (!terminalPolicy) return ::media::Status::failure(terminalPolicy.error());
            if (terminalPolicy.value() != "repeat_last_observed_positive_delta" ||
                options->has("rtp_clock_binder.source_identity"))
                return invalid("RTP interval binder requires only its planned terminal duration policy");
            videoTiming.emplace(MediaRtpVideoTimingPlan{
                MediaRtpVideoTimingProduct::CanonicalInterval, std::nullopt});
        } else if (product.value() == "source_presentation") {
            if (options->has("rtp_clock_binder.terminal_duration_policy"))
                return invalid("RTP source presentation forbids inferred terminal duration");
            auto identity = requiredNodeOption(options, "MediaRtpPacketClockBinderNode",
                "rtp_clock_binder.source_identity");
            if (!identity) return ::media::Status::failure(identity.error());
            auto copied = MediaCanonicalIdentity::copy(identity.value());
            if (!copied) return ::media::Status::failure(copied.error());
            videoTiming.emplace(MediaRtpVideoTimingPlan{
                MediaRtpVideoTimingProduct::SourcePresentation, std::move(copied).value()});
        } else {
            return invalid("RTP video binder rejects unknown video timing product");
        }
        videoClockRate = clockRate.value();
    } else if (options->has("rtp_clock_binder.video_timing_product") ||
               options->has("rtp_clock_binder.source_identity") ||
               options->has("rtp_clock_binder.video_clock_rate") ||
               options->has("rtp_clock_binder.terminal_duration_policy")) {
        return invalid("RTP audio binder rejects video timing options");
    }
    if (!m_syncGroup->plan().members ||
        (m_streamKind == MediaStreamKind::Audio &&
         *m_syncGroup->plan().members != MediaTranscodeStreamSet::AudioVideo))
        return invalid("RTP packet binder requires its stream in the planned source members");
    auto session = MediaRtpPacketClockSession::create({m_streamKind,
        *m_syncGroup->plan().members, static_cast<std::size_t>(capacity.value()),
        MediaRunningTime::fromNanoseconds(timeout.value()), videoClockRate, videoTiming},
        MediaRuntimeLineageAllocation{});
    if (!session) return ::media::Status::failure(session.error());
    if (m_preparedSource) {
        if (m_preparedSource->clock() != m_syncGroup->clock())
            return invalid("Prepared RTP binder lost its original master clock");
        auto prepared = m_preparedSource->takePacketClock({m_streamKind, *m_syncGroup->plan().members,
            static_cast<std::size_t>(capacity.value()), MediaRunningTime::fromNanoseconds(timeout.value()), videoClockRate, videoTiming});
        if (!prepared) return ::media::Status::failure(prepared.error());
        const auto& plan = m_streamKind == MediaStreamKind::Video
            ? m_preparedSource->plan().video.binder : m_preparedSource->plan().audio->binder;
        if (plan.members != *m_syncGroup->plan().members || plan.videoClockRate != videoClockRate ||
            plan.videoTiming != videoTiming)
            return invalid("Prepared RTP binder differs from runtime stream facts");
        // The prepared acquisition queue is admitted separately by its retention
        // product; once it drains, the runtime queue keeps its planned capacity.
        m_clockSession.emplace(std::move(prepared.value().session));
        m_preparedPrefix = std::move(prepared.value().prefix);
    } else m_clockSession.emplace(std::move(session).value());
    m_configured = true;
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::acceptClock(
    const MediaBufferRef& buffer)
{
    const auto* group = dynamic_cast<const MediaRtpClockGroupBuffer*>(buffer.get());
    if (!group) return invalid("RTP packet binder clock input requires a group snapshot");
    const MediaRtpClockGroupSnapshot& snapshot = group->snapshot();
    if (snapshot.state != MediaRtpClockGroupState::Locked && m_clockSession->m_lockedSnapshot) {
        logAcquisitionBoundary("clock_invalidated", nodeId(), *m_syncGroup,
            m_clockSession->m_acquiringPackets->empty() ? MediaBufferRef{} : m_clockSession->m_acquiringPackets->front(),
            m_clockSession->m_acquiringPackets->size(), m_clockSession->m_lockedSnapshot);
    }
    return m_clockSession->acceptClock(snapshot, *m_syncGroup->plan().members);
}

::media::Status MediaRtpPacketClockBinderNode::bufferAcquiring(
    MediaBufferRef buffer)
{
    const bool establishing = !m_clockSession->m_acquisitionDeadline->deadline();
    std::optional<MediaRunningTime> now;
    if (establishing) {
        auto observed = m_syncGroup->clock()->now();
        if (!observed) return ::media::Status::failure(observed.error());
        now = observed.value();
    }
    auto status = m_clockSession->bufferAcquiring(buffer, now);
    if (status && establishing) {
        logAcquisitionBoundary("deadline_established", nodeId(), *m_syncGroup,
            buffer, m_clockSession->m_acquiringPackets->size() - 1, m_clockSession->m_lockedSnapshot);
    }
    return status;
}

::media::Status MediaRtpPacketClockBinderNode::bindPacket(
    MediaGraphExecutionContext& context, MediaBufferRef buffer)
{
    auto output = m_clockSession->bindPacket(std::move(buffer));
    if (!output) return ::media::Status::failure(output.error());
    return output.value() ? emitPacket(context, output.value())
                          : ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::finishPacketInput(
    MediaGraphExecutionContext& context, MediaBufferRef terminal)
{
    auto output = m_clockSession->finishPacketInput(std::move(terminal));
    if (!output) return ::media::Status::failure(output.error());
    return output.value() ? emitPacket(context, output.value())
                          : ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::emitPacket(
    MediaGraphExecutionContext& context, MediaBufferRef buffer)
{
    const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get());
    const MediaBufferRef& owner = source ? source->media() : buffer;
    if (!dynamic_cast<FFmpegPacketBuffer*>(owner.get()))
        return invalid("RTP binder output requires one raw packet payload owner");
    if (!owner->payloadCredit() && m_preparedSource) {
        m_preparedPendingCredit = buffer;
        const auto bytes = owner->payloadFootprintBytes();
        if (!bytes) return invalid("Prepared binder output lacks payload ownership facts");
        auto reservation = context.reservePayloadBatch(nodeId(), m_streamKind,
            MediaPayloadKind::Packet, std::span<const std::uint64_t>(&*bytes, 1));
        if (!reservation) return ::media::Status::failure(reservation.error());
        if (auto status = reservation.value().front().attachTo(*owner); !status) return status;
    }
    m_preparedPendingCredit.reset();
    return emitOutput(context, "packet", buffer);
}

::media::Status MediaRtpPacketClockBinderNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpPacketClockBinderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaRtpPacketClockBinderNode::resetState() noexcept
{
    m_ownerPurge->stop();
    m_clockSession.reset();
    if (m_preparedPrefix) m_preparedPrefix->packets.clear();
    m_preparedPendingCredit.reset();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_configured = false;
}

} // namespace media::ffmpeg::graph
