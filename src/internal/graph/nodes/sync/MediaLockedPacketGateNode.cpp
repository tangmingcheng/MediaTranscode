#include "internal/graph/nodes/sync/MediaLockedPacketGateNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

using GateDispositionResult =
    ::media::Result<MediaLockedPacketGateDisposition>;

GateDispositionResult invalidDisposition(const char* message)
{
    return GateDispositionResult::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

bool transitionActive(MediaAvReacquisitionPhase phase) noexcept
{
    return phase == MediaAvReacquisitionPhase::Purging ||
        phase == MediaAvReacquisitionPhase::Acquiring ||
        phase == MediaAvReacquisitionPhase::ReadyForActivation;
}

GateDispositionResult classifyActiveTransition(
    const MediaAvReacquisitionSnapshot& snapshot,
    std::uint64_t generation)
{
    if (!transitionActive(snapshot.phase) || !snapshot.transition) {
        return invalidDisposition(
            "Locked packet gate requires a complete active transition");
    }
    if (generation == snapshot.transition->oldGeneration) {
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::DropOldGeneration);
    }
    if (generation == snapshot.transition->nextGeneration) {
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::WithholdForReacquisition);
    }
    return invalidDisposition(
        "Locked packet gate rejects an unplanned transition generation");
}

} // namespace

MediaLockedPacketGateNode::MediaLockedPacketGateNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaLockedPacketGateNode")
{
}

MediaNodeKind MediaLockedPacketGateNode::staticKind() noexcept
{
    return MediaNodeKind::LockedPacketGate;
}

::media::Result<MediaNodeProcessResult>
MediaLockedPacketGateNode::onProcess(MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (m_acquisitionDeadline->deadline()) {
        auto now = m_syncGroup->clock()->now();
        if (!now) {
            return ::media::Result<MediaNodeProcessResult>::failure(now.error());
        }
        if (auto status = m_acquisitionDeadline->preflight(now.value()); !status) {
            return processProgress(status);
        }
    }

    auto clock = tryPopInputOptional(context, "clock");
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (clock.value()) {
        auto disposition = acceptClock(*clock.value());
        return disposition
            ? processProgress()
            : processProgress(
                  ::media::Status::failure(disposition.error()));
    }
    if (m_pendingPacket) {
        if (!m_lockedGeneration) {
            if (m_acquisitionDeadline->deadline()) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waitingUntil(
                        *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
            }
            return processWaiting();
        }
        auto disposition = classifyPacket(m_pendingPacket);
        if (!disposition) {
            return processProgress(
                ::media::Status::failure(disposition.error()));
        }
        if (disposition.value() ==
            MediaLockedPacketGateDisposition::WithholdForReacquisition) {
            return processWaiting();
        }
        MediaBufferRef pending = std::move(m_pendingPacket);
        if (disposition.value() ==
            MediaLockedPacketGateDisposition::DropOldGeneration) {
            return processProgress();
        }
        return processProgress(
            emitOutput(context, "packet", pending));
    }
    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
    }
    if (!packet.value()) {
        if (m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
    MediaBufferRef input = std::move(*packet.value());
    if (input->isFlush()) {
        return processProgress(invalid(
            "Locked packet gate rejects discontinuity flush"));
    }
    if (input->isEof()) {
        if (!m_lockedGeneration) {
            return processProgress(invalid(
                "Locked packet gate cannot finish before initial lock"));
        }
        return processFinished(emitOutput(context, "packet", input));
    }
    if (!m_lockedGeneration) {
        auto generation = packetGeneration(input);
        if (!generation) {
            return processProgress(
                ::media::Status::failure(generation.error()));
        }
        if (auto status = retainPendingPacket(std::move(input)); !status) {
            return processProgress(status);
        }
        if (m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
    return processProgress(processPacket(context, std::move(input)));
}

::media::Status MediaLockedPacketGateNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_configured) return ::media::Status::success();
    const MediaNodeOptions* options = nodeOptions(context);
    auto stream = requiredStreamKindNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.stream");
    auto timeout = requiredPositiveInt64NodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.acquiring_timeout_ns");
    auto group = requiredNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.sync_group");
    if (!stream) return ::media::Status::failure(stream.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    if (!group) return ::media::Status::failure(group.error());
    m_streamKind = stream.value();
    auto deadline = MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(timeout.value()));
    if (!deadline) return ::media::Status::failure(deadline.error());
    m_acquisitionDeadline.emplace(std::move(deadline).value());
    m_syncGroupKey.emplace(std::move(group).value());
    m_syncGroup = context.findAvSyncGroup(*m_syncGroupKey);
    if (!m_syncGroup || !m_syncGroup->clock()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Locked packet gate requires registered master clock"));
    }
    m_configured = true;
    return ::media::Status::success();
}

::media::Result<MediaLockedPacketGateDisposition>
MediaLockedPacketGateNode::acceptClock(const MediaBufferRef& buffer)
{
    if (buffer->isEof()) {
        return m_lockedGeneration
            ? GateDispositionResult::success(
                  MediaLockedPacketGateDisposition::Pass)
            : invalidDisposition(
                  "Locked packet gate clock ended before lock");
    }
    const auto* state =
        dynamic_cast<const MediaSourceClockStateBuffer*>(buffer.get());
    if (!state) {
        return invalidDisposition(
            "Locked packet gate requires source clock state");
    }

    const bool discontinuity =
        hasFlag(state->flags(), MediaBufferFlag::Discontinuity);
    if (discontinuity) {
        if (!m_lockedGeneration ||
            state->readiness() !=
                MediaSourceClockReadiness::ReacquireRequired ||
            state->generation() == 0 ||
            state->generation() != *m_lockedGeneration) {
            return invalidDisposition(
                "Locked packet gate rejects malformed discontinuity evidence");
        }
        const auto snapshot = m_syncGroup->reacquisitionSnapshot();
        if (transitionActive(snapshot.phase)) {
            if (!snapshot.transition ||
                snapshot.transition->oldGeneration != state->generation() ||
                snapshot.reason !=
                    MediaAvReacquisitionReason::HardDiscontinuity) {
                return invalidDisposition(
                    "Locked packet gate rejects incompatible discontinuity evidence");
            }
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::WithholdForReacquisition);
        }
        if (snapshot.phase != MediaAvReacquisitionPhase::Inactive ||
            snapshot.transition) {
            return invalidDisposition(
                "Locked packet gate rejects discontinuity outside a live group");
        }
        auto observed = m_syncGroup->observeGeneration(state->generation());
        if (!observed ||
            observed.value() !=
                MediaAvSyncGroupRuntime::GenerationDisposition::Current) {
            return observed
                ? invalidDisposition(
                      "Locked packet gate discontinuity requires the active generation")
                : GateDispositionResult::failure(observed.error());
        }
        auto requested = m_syncGroup->requestReacquisition(
            MediaAvReacquisitionRequest{
                state->generation(),
                MediaAvReacquisitionReason::HardDiscontinuity});
        if (!requested) {
            return GateDispositionResult::failure(requested.error());
        }
        const auto requestedSnapshot =
            m_syncGroup->reacquisitionSnapshot();
        if (!transitionActive(requestedSnapshot.phase) ||
            !requestedSnapshot.transition ||
            requestedSnapshot.transition->oldGeneration !=
                state->generation()) {
            return invalidDisposition(
                "Locked packet gate requires the requested group transition");
        }
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::WithholdForReacquisition);
    }
    if (state->readiness() ==
        MediaSourceClockReadiness::ReacquireRequired) {
        return invalidDisposition(
            "Locked packet gate rejects unmarked reacquisition evidence");
    }
    if (state->readiness() == MediaSourceClockReadiness::Acquiring) {
        if (!m_lockedGeneration) {
            auto now = m_syncGroup->clock()->now();
            if (!now) return GateDispositionResult::failure(now.error());
            auto established =
                m_acquisitionDeadline->establish(now.value());
            return established
                ? GateDispositionResult::success(
                      MediaLockedPacketGateDisposition::
                          WithholdForReacquisition)
                : GateDispositionResult::failure(established.error());
        }
        const auto snapshot = m_syncGroup->reacquisitionSnapshot();
        if ((snapshot.phase != MediaAvReacquisitionPhase::Acquiring &&
             snapshot.phase !=
                 MediaAvReacquisitionPhase::ReadyForActivation) ||
            !snapshot.transition ||
            state->generation() !=
                snapshot.transition->nextGeneration) {
            return invalidDisposition(
                "Locked packet gate rejects acquiring evidence without the active transition");
        }
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::WithholdForReacquisition);
    }
    if (state->readiness() != MediaSourceClockReadiness::Locked) {
        return invalidDisposition(
            "Locked packet gate rejects degraded clock evidence");
    }
    if (state->generation() == 0) {
        return invalidDisposition(
            "Locked packet gate requires nonzero locked generation");
    }
    if (!m_lockedGeneration) {
        m_lockedGeneration = state->generation();
        m_acquisitionDeadline->clear();
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::Pass);
    }
    return classifyLockedGeneration(state->generation(), true);
}

::media::Result<MediaLockedPacketGateDisposition>
MediaLockedPacketGateNode::classifyLockedGeneration(
    std::uint64_t generation,
    bool mayRequestReacquisition)
{
    if (generation == 0 || !m_lockedGeneration) {
        return invalidDisposition(
            "Locked packet gate requires an active nonzero generation");
    }
    const auto snapshot = m_syncGroup->reacquisitionSnapshot();
    if (transitionActive(snapshot.phase)) {
        return classifyActiveTransition(snapshot, generation);
    }
    if (snapshot.phase != MediaAvReacquisitionPhase::Inactive ||
        snapshot.transition) {
        return invalidDisposition(
            "Locked packet gate rejects generation evidence outside a live group");
    }
    if (generation < *m_lockedGeneration) {
        return invalidDisposition(
            "Locked packet gate rejects generation regression");
    }
    if (generation == *m_lockedGeneration) {
        const auto epoch = m_syncGroup->epochTransitionSnapshot();
        if (!epoch.playbackEpoch) {
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::Pass);
        }
        auto observed = m_syncGroup->observeGeneration(generation);
        if (!observed) {
            return GateDispositionResult::failure(observed.error());
        }
        if (observed.value() ==
            MediaAvSyncGroupRuntime::GenerationDisposition::Current) {
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::Pass);
        }
        const auto concurrentSnapshot =
            m_syncGroup->reacquisitionSnapshot();
        return transitionActive(concurrentSnapshot.phase)
            ? classifyActiveTransition(concurrentSnapshot, generation)
            : invalidDisposition(
                  "Locked packet gate rejects non-current active generation");
    }
    if (!mayRequestReacquisition) {
        return invalidDisposition(
            "Locked packet gate rejects future packet generation without a transition");
    }
    auto observed = m_syncGroup->observeGeneration(generation);
    if (!observed) {
        return GateDispositionResult::failure(observed.error());
    }
    if (observed.value() !=
        MediaAvSyncGroupRuntime::GenerationDisposition::
            ReacquisitionRequired) {
        return invalidDisposition(
            "Locked packet gate rejects an unexpected future generation");
    }
    auto requested = m_syncGroup->requestReacquisition(
        MediaAvReacquisitionRequest{
            generation,
            MediaAvReacquisitionReason::FutureGeneration});
    if (!requested) {
        return GateDispositionResult::failure(requested.error());
    }
    return classifyActiveTransition(
        m_syncGroup->reacquisitionSnapshot(), generation);
}

::media::Result<std::uint64_t>
MediaLockedPacketGateNode::packetGeneration(const MediaBufferRef& buffer) const
{
    const auto* packet =
        dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->sourceTiming() ||
        packet->streamKind() != m_streamKind ||
        packet->sourceTiming()->readiness !=
            MediaSourceClockReadiness::Locked ||
        packet->sourceTiming()->generation == 0 ||
        hasFlag(packet->flags(), MediaBufferFlag::Discontinuity)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Locked packet gate rejects packet clock evidence"));
    }
    return ::media::Result<std::uint64_t>::success(
        packet->sourceTiming()->generation);
}

::media::Result<MediaLockedPacketGateDisposition>
MediaLockedPacketGateNode::classifyPacket(const MediaBufferRef& buffer)
{
    auto generation = packetGeneration(buffer);
    return generation
        ? classifyLockedGeneration(generation.value(), false)
        : GateDispositionResult::failure(generation.error());
}

::media::Status MediaLockedPacketGateNode::processPacket(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    auto disposition = classifyPacket(buffer);
    if (!disposition) {
        return ::media::Status::failure(disposition.error());
    }
    switch (disposition.value()) {
    case MediaLockedPacketGateDisposition::Pass:
        return emitOutput(context, "packet", buffer);
    case MediaLockedPacketGateDisposition::WithholdForReacquisition:
        return retainPendingPacket(std::move(buffer));
    case MediaLockedPacketGateDisposition::DropOldGeneration:
        return ::media::Status::success();
    }
    return invalid("Locked packet gate rejects unknown disposition");
}

::media::Status MediaLockedPacketGateNode::retainPendingPacket(
    MediaBufferRef buffer)
{
    if (m_pendingPacket || !buffer) {
        return invalid(
            "Locked packet gate retains at most one pending packet");
    }
    m_pendingPacket = std::move(buffer);
    return ::media::Status::success();
}

::media::Status MediaLockedPacketGateNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaLockedPacketGateNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaLockedPacketGateNode::resetState() noexcept
{
    m_lockedGeneration.reset();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_acquisitionDeadline.reset();
    m_pendingPacket.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_configured = false;
}

} // namespace media::ffmpeg::graph
