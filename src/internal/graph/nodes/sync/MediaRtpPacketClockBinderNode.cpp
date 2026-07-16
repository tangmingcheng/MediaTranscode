#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavutil/avutil.h>
}

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

MediaRtpPacketClockBinderNode::MediaRtpPacketClockBinderNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpPacketClockBinderNode")
{
}

MediaNodeKind MediaRtpPacketClockBinderNode::staticKind() noexcept
{
    return MediaNodeKind::RtpPacketClockBinder;
}

::media::Result<MediaNodeProcessResult> MediaRtpPacketClockBinderNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (m_pendingTerminal) {
        MediaBufferRef terminal = std::move(m_pendingTerminal);
        return terminal->isEof()
            ? processFinished(emitOutput(context, "packet", terminal))
            : processProgress(emitOutput(context, "packet", terminal));
    }

    auto clock = tryPopInputOptional(context, "clock");
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (clock.value()) {
        return processProgress(acceptClock(*clock.value()));
    }

    if (m_lockedSnapshot && !m_acquiringPackets.empty()) {
        MediaBufferRef buffered = std::move(m_acquiringPackets.front());
        m_acquiringPackets.pop_front();
        return processProgress(bindPacket(context, std::move(buffered)));
    }

    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
    }
    if (!packet.value()) return processWaiting();
    MediaBufferRef input = std::move(*packet.value());
    if (input->isEof() || input->isFlush()) {
        return processProgress(finishPacketInput(context, std::move(input)));
    }
    if (!m_lockedSnapshot) {
        return processProgress(bufferAcquiring(std::move(input)));
    }
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
    if (!stream) return ::media::Status::failure(stream.error());
    if (!capacity) return ::media::Status::failure(capacity.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    m_streamKind = stream.value();
    m_scheduledStream = m_streamKind == MediaStreamKind::Video
        ? MediaScheduledStream::Video
        : MediaScheduledStream::Audio;
    m_acquiringCapacity = static_cast<std::size_t>(capacity.value());
    m_acquiringTimeout = std::chrono::nanoseconds(timeout.value());
    if (m_streamKind == MediaStreamKind::Video) {
        auto clockRate = requiredPositiveIntNodeOption(
            options, "MediaRtpPacketClockBinderNode",
            "rtp_clock_binder.duration_clock_rate");
        auto terminalPolicy = requiredNodeOption(
            options, "MediaRtpPacketClockBinderNode",
            "rtp_clock_binder.terminal_duration_policy");
        if (!clockRate) return ::media::Status::failure(clockRate.error());
        if (!terminalPolicy) {
            return ::media::Status::failure(terminalPolicy.error());
        }
        if (terminalPolicy.value() != "repeat_last_observed_positive_delta") {
            return invalid("RTP video binder rejects unplanned terminal duration policy");
        }
        m_durationClockRate = clockRate.value();
    }
    m_configured = true;
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::acceptClock(
    const MediaBufferRef& buffer)
{
    const auto* group = dynamic_cast<const MediaRtpClockGroupBuffer*>(buffer.get());
    if (!group) return invalid("RTP packet binder clock input requires a group snapshot");
    const MediaRtpClockGroupSnapshot& snapshot = group->snapshot();
    const bool discriminated =
        (snapshot.state == MediaRtpClockGroupState::Locked) ==
        snapshot.locked.has_value();
    if (!discriminated) {
        return invalid("RTP packet binder rejects malformed clock group snapshot");
    }
    if (snapshot.state == MediaRtpClockGroupState::Acquiring) {
        if (m_lockedGeneration) {
            return invalid("RTP packet binder rejects acquiring after locked generation");
        }
        return ::media::Status::success();
    }
    if (snapshot.state != MediaRtpClockGroupState::Locked ||
        snapshot.groupGeneration == 0) {
        return invalid("RTP packet binder requires locked clock evidence");
    }
    if (m_lockedGeneration && *m_lockedGeneration != snapshot.groupGeneration) {
        return invalid("RTP packet binder rejects stale or future clock generation");
    }
    m_lockedGeneration = snapshot.groupGeneration;
    m_lockedSnapshot = snapshot;
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::bufferAcquiring(
    MediaBufferRef buffer)
{
    const auto now = std::chrono::steady_clock::now();
    if (!m_acquiringStarted) m_acquiringStarted = now;
    if (now - *m_acquiringStarted > m_acquiringTimeout) {
        return invalid("RTP packet binder acquiring timeout expired");
    }
    if (m_acquiringPackets.size() >= m_acquiringCapacity) {
        return invalid("RTP packet binder acquiring capacity exhausted");
    }
    m_acquiringPackets.push_back(std::move(buffer));
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> MediaRtpPacketClockBinderNode::timedPacket(
    MediaBufferRef buffer,
    std::uint64_t& extendedTimestamp)
{
    auto* source = dynamic_cast<FFmpegPacketBuffer*>(buffer.get());
    if (!source || !source->packet() || source->sourceTiming() ||
        source->streamKind() != m_streamKind ||
        source->packet()->pts == AV_NOPTS_VALUE || source->packet()->pts < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet binder requires untimed matching packets with extended RTP PTS"));
    }
    extendedTimestamp = static_cast<std::uint64_t>(source->packet()->pts);
    auto timing = m_projector.project(
        *m_lockedSnapshot, m_scheduledStream, extendedTimestamp);
    if (!timing) {
        return ::media::Result<MediaBufferRef>::failure(timing.error());
    }
    const MediaFormatDescriptor format = source->formatDescriptor();
    const MediaTimeDescriptor time = source->timeDescriptor();
    const MediaHardwareDescriptor hardware = source->hardwareDescriptor();
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        source->takePacket(), m_streamKind, std::move(timing).value());
    if (!wrapped) return wrapped;
    wrapped.value()->setFormatDescriptor(format);
    wrapped.value()->setTimeDescriptor(time);
    wrapped.value()->setHardwareDescriptor(hardware);
    return wrapped;
}

::media::Status MediaRtpPacketClockBinderNode::bindPacket(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    std::uint64_t timestamp = 0;
    auto timed = timedPacket(std::move(buffer), timestamp);
    if (!timed) return ::media::Status::failure(timed.error());
    if (m_streamKind == MediaStreamKind::Audio) {
        return emitOutput(context, "packet", timed.value());
    }
    if (!m_videoLookahead) {
        m_videoLookahead = std::move(timed).value();
        m_videoLookaheadTimestamp = timestamp;
        return ::media::Status::success();
    }
    if (!m_videoLookaheadTimestamp || timestamp <= *m_videoLookaheadTimestamp ||
        timestamp - *m_videoLookaheadTimestamp >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return invalid("RTP video binder requires positive representable timestamp delta");
    }
    const std::int64_t delta = static_cast<std::int64_t>(
        timestamp - *m_videoLookaheadTimestamp);
    auto* prior = dynamic_cast<FFmpegPacketBuffer*>(m_videoLookahead.get());
    if (!prior || !prior->packet()) {
        return invalid("RTP video binder lost lookahead packet ownership");
    }
    prior->packet()->duration = delta;
    m_videoLookahead->setTimestamps(
        prior->packet()->pts, prior->packet()->dts, delta);
    MediaBufferRef output = std::move(m_videoLookahead);
    m_videoLookahead = std::move(timed).value();
    m_videoLookaheadTimestamp = timestamp;
    m_lastPositiveVideoDelta = delta;
    return emitOutput(context, "packet", output);
}

::media::Status MediaRtpPacketClockBinderNode::finishPacketInput(
    MediaGraphExecutionContext& context,
    MediaBufferRef terminal)
{
    if (!m_lockedSnapshot || !m_acquiringPackets.empty()) {
        return invalid("RTP packet binder cannot terminate before buffered packets are clock-bound");
    }
    if (m_streamKind == MediaStreamKind::Video && m_videoLookahead) {
        if (!m_lastPositiveVideoDelta) {
            return invalid("RTP video binder terminal packet has no observed positive duration");
        }
        auto* packet = dynamic_cast<FFmpegPacketBuffer*>(m_videoLookahead.get());
        if (!packet || !packet->packet()) {
            return invalid("RTP video binder lost terminal lookahead packet ownership");
        }
        packet->packet()->duration = *m_lastPositiveVideoDelta;
        m_videoLookahead->setTimestamps(
            packet->packet()->pts, packet->packet()->dts,
            *m_lastPositiveVideoDelta);
        MediaBufferRef output = std::move(m_videoLookahead);
        m_videoLookaheadTimestamp.reset();
        m_pendingTerminal = std::move(terminal);
        return emitOutput(context, "packet", output);
    }
    m_pendingTerminal = std::move(terminal);
    return ::media::Status::success();
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
    m_lockedSnapshot.reset();
    m_lockedGeneration.reset();
    m_acquiringPackets.clear();
    m_acquiringStarted.reset();
    m_videoLookahead.reset();
    m_videoLookaheadTimestamp.reset();
    m_lastPositiveVideoDelta.reset();
    m_pendingTerminal.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_scheduledStream = MediaScheduledStream::Video;
    m_acquiringCapacity = 0;
    m_acquiringTimeout = std::chrono::nanoseconds(0);
    m_durationClockRate = 0;
    m_configured = false;
}

} // namespace media::ffmpeg::graph
