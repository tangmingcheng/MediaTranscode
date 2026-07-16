#include "internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaInitialLockedPacketGateNode::MediaInitialLockedPacketGateNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaInitialLockedPacketGateNode")
{
}

MediaNodeKind MediaInitialLockedPacketGateNode::staticKind() noexcept
{
    return MediaNodeKind::InitialLockedPacketGate;
}

::media::Result<MediaNodeProcessResult>
MediaInitialLockedPacketGateNode::onProcess(MediaGraphExecutionContext& context)
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
        return processProgress(acceptClock(*clock.value()));
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
            "Initial locked packet gate rejects discontinuity flush"));
    }
    if (input->isEof()) {
        if (!m_lockedGeneration) {
            return processProgress(invalid(
                "Initial locked packet gate cannot finish before initial lock"));
        }
        return processFinished(emitOutput(context, "packet", input));
    }
    if (!m_lockedGeneration)
        return processProgress(invalid(
            "Initial locked packet gate rejects packet before clock lock"));
    return processProgress(emitValidatedPacket(context, std::move(input)));
}

::media::Status MediaInitialLockedPacketGateNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_configured) return ::media::Status::success();
    const MediaNodeOptions* options = nodeOptions(context);
    auto stream = requiredStreamKindNodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.stream");
    auto timeout = requiredPositiveInt64NodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.acquiring_timeout_ns");
    auto group = requiredNodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.sync_group");
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
            "Initial locked packet gate requires registered master clock"));
    }
    m_configured = true;
    return ::media::Status::success();
}

::media::Status MediaInitialLockedPacketGateNode::acceptClock(
    const MediaBufferRef& buffer)
{
    if (buffer->isEof()) {
        return m_lockedGeneration
            ? ::media::Status::success()
            : invalid("Initial locked packet gate clock ended before lock");
    }
    const auto* state = dynamic_cast<const MediaSourceClockStateBuffer*>(buffer.get());
    if (!state) {
        return invalid("Initial locked packet gate requires source clock state");
    }
    if (hasFlag(state->flags(), MediaBufferFlag::Discontinuity)) {
        return invalid("Initial locked packet gate rejects clock discontinuity");
    }
    if (state->readiness() == MediaSourceClockReadiness::Acquiring) {
        if (m_lockedGeneration) {
            return invalid("Initial locked packet gate rejects reacquisition");
        }
        auto now = m_syncGroup->clock()->now();
        if (!now) return ::media::Status::failure(now.error());
        return m_acquisitionDeadline->establish(now.value());
    }
    if (state->readiness() == MediaSourceClockReadiness::Locked &&
        state->generation() == 0) {
        return invalid("Initial locked packet gate requires nonzero locked generation");
    }
    if (state->readiness() != MediaSourceClockReadiness::Locked) {
        return invalid("Initial locked packet gate rejects degraded or reacquire evidence");
    }
    if (m_lockedGeneration && *m_lockedGeneration != state->generation()) {
        return invalid("Initial locked packet gate rejects generation change");
    }
    m_lockedGeneration = state->generation();
    m_acquisitionDeadline->clear();
    return ::media::Status::success();
}

::media::Status MediaInitialLockedPacketGateNode::emitValidatedPacket(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->sourceTiming() ||
        packet->streamKind() != m_streamKind ||
        packet->sourceTiming()->readiness != MediaSourceClockReadiness::Locked ||
        packet->sourceTiming()->generation != *m_lockedGeneration ||
        hasFlag(packet->flags(), MediaBufferFlag::Discontinuity)) {
        return invalid("Initial locked packet gate rejects packet clock evidence");
    }
    return emitOutput(context, "packet", buffer);
}

::media::Status MediaInitialLockedPacketGateNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaInitialLockedPacketGateNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaInitialLockedPacketGateNode::resetState() noexcept
{
    m_lockedGeneration.reset();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_acquisitionDeadline.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_configured = false;
}

} // namespace media::ffmpeg::graph
