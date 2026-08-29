#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

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
    if (m_acquisitionDeadline->deadline()) {
        auto now = m_syncGroup->clock()->now();
        if (!now) {
            return ::media::Result<MediaNodeProcessResult>::failure(now.error());
        }
        if (auto status = m_acquisitionDeadline->preflight(now.value()); !status) {
            return processProgress(status);
        }
    }
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
    if (!packet.value()) {
        if (m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntilInputOrDeadline(
                    *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
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
    auto groupName = requiredNodeOption(
        options, "MediaRtpPacketClockBinderNode",
        "rtp_clock_binder.sync_group");
    if (!stream) return ::media::Status::failure(stream.error());
    if (!capacity) return ::media::Status::failure(capacity.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    if (!groupName) return ::media::Status::failure(groupName.error());
    m_streamKind = stream.value();
    m_scheduledStream = m_streamKind == MediaStreamKind::Video
        ? MediaScheduledStream::Video
        : MediaScheduledStream::Audio;
    m_acquiringCapacity = static_cast<std::size_t>(capacity.value());
    auto deadline = MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(timeout.value()));
    if (!deadline) return ::media::Status::failure(deadline.error());
    m_acquisitionDeadline.emplace(std::move(deadline).value());
    m_syncGroupKey.emplace(std::move(groupName).value());
    m_syncGroup = context.findAvSyncGroup(*m_syncGroupKey);
    if (!m_syncGroup || !m_syncGroup->clock()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP packet binder requires a registered sync-group master clock"));
    }
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
    if (snapshot.state != MediaRtpClockGroupState::Locked) {
        if (m_lockedSnapshot) invalidateClockProjection();
        return ::media::Status::success();
    }
    if (snapshot.groupGeneration == 0) {
        return invalid("RTP packet binder requires locked clock evidence");
    }
    if (m_lockedSnapshot &&
        m_lockedSnapshot->groupGeneration != snapshot.groupGeneration) {
        invalidateClockProjection();
    }
    m_lockedSnapshot = snapshot;
    m_acquisitionDeadline->clear();
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockBinderNode::bufferAcquiring(
    MediaBufferRef buffer)
{
    if (!m_acquisitionDeadline->deadline()) {
        auto now = m_syncGroup->clock()->now();
        if (!now) return ::media::Status::failure(now.error());
        if (auto status = m_acquisitionDeadline->establish(now.value()); !status) {
            return status;
        }
    }
    if (m_acquiringPackets.size() >= m_acquiringCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP packet binder acquiring capacity exhausted: buffered=" +
            std::to_string(m_acquiringPackets.size()) +
            " capacity=" + std::to_string(m_acquiringCapacity)));
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
        source->packet()->pts == AV_NOPTS_VALUE || source->packet()->pts < 0 ||
        source->packet()->pts > std::numeric_limits<std::uint32_t>::max() ||
        (source->packet()->dts != AV_NOPTS_VALUE &&
         source->packet()->dts != source->packet()->pts)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet binder requires untimed matching packets with raw 32-bit RTP PTS and no distinct DTS"));
    }
    const MediaTimeDescriptor time = source->timeDescriptor();
    const MediaRtpSourceClockCalibration& calibration =
        m_scheduledStream == MediaScheduledStream::Video
        ? m_lockedSnapshot->locked->video
        : m_lockedSnapshot->locked->audio;
    if (!time.hasKnownTimeBase() || time.timeBase.num != 1 ||
        (m_streamKind == MediaStreamKind::Video &&
         time.timeBase.den != m_durationClockRate)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet binder requires the planned packet time base"));
    }
    auto aligned = m_timestampAligner.align(
        calibration, static_cast<std::uint32_t>(source->packet()->pts));
    if (!aligned) {
        return ::media::Result<MediaBufferRef>::failure(aligned.error());
    }
    extendedTimestamp = aligned.value();
    auto timing = m_projector.project(
        *m_lockedSnapshot, m_scheduledStream, extendedTimestamp);
    if (!timing) {
        return ::media::Result<MediaBufferRef>::failure(timing.error());
    }
    auto packet = source->takePacket();
    const std::int64_t materializedTimestamp =
        static_cast<std::int64_t>(extendedTimestamp);
    packet->pts = materializedTimestamp;
    if (packet->dts != AV_NOPTS_VALUE) {
        packet->dts = materializedTimestamp;
    }
    const MediaFormatDescriptor format = source->formatDescriptor();
    const MediaHardwareDescriptor hardware = source->hardwareDescriptor();
    const bool inputKey = source->isKeyFrame();
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), m_streamKind, std::move(timing).value());
    if (!wrapped) return wrapped;
    if (m_streamKind == MediaStreamKind::Video && inputKey &&
        !m_keyTraceEmitted) {
        m_keyTraceEmitted = true;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("rtp_key_trace stage=clock_binder input=") +
                (inputKey ? "1" : "0") + " output=" +
                (wrapped.value()->isKeyFrame() ? "1" : "0"));
    }
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

void MediaRtpPacketClockBinderNode::invalidateClockProjection() noexcept
{
    m_lockedSnapshot.reset();
    m_videoLookahead.reset();
    m_videoLookaheadTimestamp.reset();
    m_lastPositiveVideoDelta.reset();
    m_pendingTerminal.reset();
}

void MediaRtpPacketClockBinderNode::resetState() noexcept
{
    invalidateClockProjection();
    m_acquiringPackets.clear();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_acquisitionDeadline.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_scheduledStream = MediaScheduledStream::Video;
    m_acquiringCapacity = 0;
    m_durationClockRate = 0;
    m_configured = false;
    m_keyTraceEmitted = false;
}

} // namespace media::ffmpeg::graph
