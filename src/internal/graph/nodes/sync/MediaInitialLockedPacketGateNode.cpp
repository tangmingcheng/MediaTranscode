#include "internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <limits>
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

    auto clock = tryPopInputOptional(context, "clock");
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (clock.value()) {
        return processProgress(acceptClock(*clock.value()));
    }
    if (auto status = checkAcquiringDeadline(); !status) {
        return processProgress(status);
    }
    if (m_lockedGeneration && !m_acquiringPackets.empty()) {
        MediaBufferRef packet = std::move(m_acquiringPackets.front());
        m_acquiringPackets.pop_front();
        return processProgress(emitValidatedPacket(context, std::move(packet)));
    }

    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
    }
    if (!packet.value()) {
        if (m_acquiringDeadline) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    *m_syncGroupKey, *m_acquiringDeadline));
        }
        return processWaiting();
    }
    MediaBufferRef input = std::move(*packet.value());
    if (input->isFlush()) {
        return processProgress(invalid(
            "Initial locked packet gate rejects discontinuity flush"));
    }
    if (input->isEof()) {
        if (!m_lockedGeneration || !m_acquiringPackets.empty()) {
            return processProgress(invalid(
                "Initial locked packet gate cannot finish before initial lock"));
        }
        return processFinished(emitOutput(context, "packet", input));
    }
    if (!m_lockedGeneration) {
        return processProgress(bufferPacket(std::move(input)));
    }
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
    auto capacity = requiredPositiveIntNodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.acquiring_capacity");
    auto timeout = requiredPositiveInt64NodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.acquiring_timeout_ns");
    auto group = requiredNodeOption(
        options, "MediaInitialLockedPacketGateNode",
        "initial_locked_gate.sync_group");
    if (!stream) return ::media::Status::failure(stream.error());
    if (!capacity) return ::media::Status::failure(capacity.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    if (!group) return ::media::Status::failure(group.error());
    m_streamKind = stream.value();
    m_acquiringCapacity = static_cast<std::size_t>(capacity.value());
    m_acquiringTimeoutNs = timeout.value();
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
    if (state->readiness() == MediaSourceClockReadiness::Acquiring ||
        (state->readiness() == MediaSourceClockReadiness::Locked &&
         state->generation() == 0)) {
        return m_lockedGeneration
            ? invalid("Initial locked packet gate rejects reacquisition")
            : ::media::Status::success();
    }
    if (state->readiness() != MediaSourceClockReadiness::Locked) {
        return invalid("Initial locked packet gate rejects degraded or reacquire evidence");
    }
    if (m_lockedGeneration && *m_lockedGeneration != state->generation()) {
        return invalid("Initial locked packet gate rejects generation change");
    }
    m_lockedGeneration = state->generation();
    m_acquiringDeadline.reset();
    return ::media::Status::success();
}

::media::Status MediaInitialLockedPacketGateNode::checkAcquiringDeadline() const
{
    if (!m_acquiringDeadline || m_lockedGeneration) {
        return ::media::Status::success();
    }
    auto now = m_syncGroup->clock()->now();
    if (!now) return ::media::Status::failure(now.error());
    if (now.value() >= *m_acquiringDeadline) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Initial locked packet gate acquiring deadline expired"));
    }
    return ::media::Status::success();
}

::media::Status MediaInitialLockedPacketGateNode::bufferPacket(
    MediaBufferRef buffer)
{
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->sourceTiming() ||
        packet->streamKind() != m_streamKind ||
        packet->sourceTiming()->readiness != MediaSourceClockReadiness::Locked ||
        packet->sourceTiming()->generation == 0 ||
        hasFlag(packet->flags(), MediaBufferFlag::Discontinuity)) {
        return invalid("Initial locked packet gate requires locked normalized packet timing");
    }
    if (!m_acquiringDeadline) {
        auto now = m_syncGroup->clock()->now();
        if (!now) return ::media::Status::failure(now.error());
        if (now.value().nanoseconds() >
            std::numeric_limits<std::int64_t>::max() - m_acquiringTimeoutNs) {
            return invalid("Initial locked packet gate deadline overflows master time");
        }
        m_acquiringDeadline = MediaRunningTime::fromNanoseconds(
            now.value().nanoseconds() + m_acquiringTimeoutNs);
    }
    if (m_acquiringPackets.size() >= m_acquiringCapacity) {
        return invalid("Initial locked packet gate acquiring capacity exhausted");
    }
    m_acquiringPackets.push_back(std::move(buffer));
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
    m_acquiringPackets.clear();
    m_lockedGeneration.reset();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_acquiringDeadline.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_acquiringCapacity = 0;
    m_acquiringTimeoutNs = 0;
    m_configured = false;
}

} // namespace media::ffmpeg::graph
