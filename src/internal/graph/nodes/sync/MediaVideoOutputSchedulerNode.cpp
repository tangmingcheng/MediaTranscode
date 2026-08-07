#include "internal/graph/nodes/sync/MediaVideoOutputSchedulerNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaOutputSchedule.h"

namespace media::ffmpeg::graph {
namespace {

constexpr MediaRational NanosecondTimeBase{1, 1'000'000'000};

} // namespace

MediaVideoOutputSchedulerNode::MediaVideoOutputSchedulerNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaVideoOutputSchedulerNode")
{
}

MediaNodeKind MediaVideoOutputSchedulerNode::staticKind() noexcept
{
    return MediaNodeKind::VideoOutputScheduler;
}

::media::Status MediaVideoOutputSchedulerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto configured = configure(context);
    if (!configured) return configured;
    m_startedAt = std::chrono::steady_clock::now();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaVideoOutputSchedulerNode::configure(
    MediaGraphExecutionContext& context)
{
    const auto* options = nodeOptions(context);
    auto requireKeyFrame = requiredBoolNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.require_key_frame");
    auto maximumWait = requiredPositiveInt64NodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_wait_ns");
    auto packetCapacity = requiredPositiveInt64NodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.packet_capacity");
    auto maximumUnitBytes = requiredPositiveInt64NodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_unit_bytes");
    auto byteCapacity = requiredPositiveInt64NodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.byte_capacity");
    auto sourceNumerator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.num");
    auto sourceDenominator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.den");
    auto frameRateNumerator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.num");
    auto frameRateDenominator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.den");
    auto packetTimeBaseNumerator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.num");
    auto packetTimeBaseDenominator = requiredPositiveIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.den");
    auto packetTimingMode = requiredNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_timing_mode");
    auto transportLead = requiredNonNegativeIntNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.transport_lead_ns");
    auto pacingEnabled = requiredBoolNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.pacing_enabled");
    if (!requireKeyFrame || !maximumWait || !packetCapacity ||
        !maximumUnitBytes || !byteCapacity || !sourceNumerator ||
        !sourceDenominator || !frameRateNumerator ||
        !frameRateDenominator || !packetTimeBaseNumerator ||
        !packetTimeBaseDenominator || !packetTimingMode ||
        !transportLead || !pacingEnabled) {
        const auto& error = !requireKeyFrame ? requireKeyFrame.error()
            : !maximumWait ? maximumWait.error()
            : !packetCapacity ? packetCapacity.error()
            : !maximumUnitBytes ? maximumUnitBytes.error()
            : !byteCapacity ? byteCapacity.error()
            : !sourceNumerator ? sourceNumerator.error()
            : !sourceDenominator ? sourceDenominator.error()
            : !frameRateNumerator ? frameRateNumerator.error()
            : !frameRateDenominator ? frameRateDenominator.error()
            : !packetTimeBaseNumerator ? packetTimeBaseNumerator.error()
            : !packetTimeBaseDenominator ? packetTimeBaseDenominator.error()
            : !packetTimingMode ? packetTimingMode.error()
            : !transportLead ? transportLead.error()
            : pacingEnabled.error();
        return ::media::Status::failure(error);
    }
    if (!pacingEnabled.value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler requires planner-enabled pacing"));
    }
    const auto inputs = context.inputChannels(nodeId());
    const auto outputs = context.outputChannels(nodeId());
    if (inputs.size() != 1 || outputs.size() != 1 ||
        !context.findInputChannel(nodeId(), "video") ||
        !context.findOutputChannel(nodeId(), "scheduled_video")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler requires exactly one video input and output"));
    }
    m_requireKeyFrame = requireKeyFrame.value();
    m_maximumStartupWait =
        MediaRunningTime::fromNanoseconds(maximumWait.value());
    m_packetCapacity = static_cast<std::size_t>(packetCapacity.value());
    m_maximumUnitBytes = static_cast<std::uint64_t>(maximumUnitBytes.value());
    m_byteCapacity = static_cast<std::uint64_t>(byteCapacity.value());
    if (m_packetCapacity > m_byteCapacity / m_maximumUnitBytes ||
        static_cast<std::uint64_t>(m_packetCapacity) * m_maximumUnitBytes !=
            m_byteCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler startup bounds are inconsistent"));
    }
    m_transportLead =
        MediaRunningTime::fromNanoseconds(transportLead.value());
    m_sourceTimeBase =
        MediaRational{sourceNumerator.value(), sourceDenominator.value()};
    m_outputFrameRate = MediaRational{
        frameRateNumerator.value(), frameRateDenominator.value()};
    m_packetTimeBase = MediaRational{
        packetTimeBaseNumerator.value(), packetTimeBaseDenominator.value()};
    if (packetTimingMode.value() == "source_time_base") {
        m_packetTimingMode = PacketTimingMode::SourceTimeBase;
        if (m_packetTimeBase.num != m_sourceTimeBase.num ||
            m_packetTimeBase.den != m_sourceTimeBase.den) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoOnly source timing mode requires the source time base"));
        }
    } else if (packetTimingMode.value() == "output_cadence_time_base") {
        m_packetTimingMode = PacketTimingMode::OutputCadenceTimeBase;
        if (m_packetTimeBase.num != m_outputFrameRate.den ||
            m_packetTimeBase.den != m_outputFrameRate.num) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "VideoOnly cadence timing mode requires the inverse output frame rate"));
        }
    } else {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler received an unknown packet timing mode"));
    }
    MediaLatencyPolicy pacing;
    pacing.mode = MediaLatencyMode::Realtime;
    pacing.enablePacing = pacingEnabled.value();
    m_pacingClock.setPolicy(pacing);
    m_configured = true;
    return ::media::Status::success();
}

::media::Status MediaVideoOutputSchedulerNode::validateStartupDeadline() const
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_startedAt);
    if (elapsed.count() <= m_maximumStartupWait.nanoseconds()) {
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::ioFailure(
        "VideoOnly scheduler exceeded the planner startup deadline"));
}

::media::Result<MediaNodeProcessResult>
MediaVideoOutputSchedulerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_configured) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly scheduler has no configured runtime product"));
    }
    if (m_pendingBuffer) return emitPending(context);
    auto input = tryPopInputOptional(context, "video");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        if (!m_startedMedia) {
            auto deadline = validateStartupDeadline();
            if (!deadline) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    deadline.error());
            }
        }
        if (!m_startedMedia) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    m_startedAt + std::chrono::nanoseconds(
                        m_maximumStartupWait.nanoseconds())));
        }
        return processWaiting();
    }
    MediaBufferRef buffer = std::move(*input.value());
    if (buffer->isEof() || buffer->isFlush()) {
        if (!m_startedMedia) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::ioFailure(
                    "VideoOnly scheduler reached end of input before its startup contract was satisfied"));
        }
        auto emitted = emitOutput(context, "scheduled_video", buffer);
        return buffer->isEof() ? processFinished(emitted)
                               : processProgress(emitted);
    }
    if (!FFmpegPacketView::isPacket(buffer) ||
        buffer->streamKind() != MediaStreamKind::Video ||
        buffer->payloadKind() != MediaPayloadKind::Packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler requires encoded video packets"));
    }
    if (!m_startedMedia) {
        auto deadline = validateStartupDeadline();
        if (!deadline) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                deadline.error());
        }
        if (m_requireKeyFrame && !buffer->isKeyFrame()) {
            return processProgress();
        }
        m_startedMedia = true;
        m_pacingClock.reset();
    }
    if (buffer->dts() == invalidMediaTimeValue ||
        !buffer->timeDescriptor().timeBase.isKnown()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly scheduler requires decode timestamps with an explicit time base"));
    }
    const auto& timeBase = buffer->timeDescriptor().timeBase;
    if (timeBase.num != m_packetTimeBase.num ||
        timeBase.den != m_packetTimeBase.den) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler packet time base disagrees with the planner timing product"));
    }
    const auto footprint = buffer->payloadFootprintBytes();
    if (!footprint || *footprint > m_maximumUnitBytes ||
        *footprint > m_byteCapacity) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler packet exceeds the planner startup byte bounds"));
    }
    auto dispatch = MediaRunningTime::checkedFromTicks(
        buffer->dts(), timeBase.num, timeBase.den);
    if (!dispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            dispatch.error());
    }
    auto schedule = MediaOutputSchedule::create(
        dispatch.value(), dispatch.value(), m_transportLead);
    if (!schedule) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            schedule.error());
    }
    auto target = m_pacingClock.targetTime(
        schedule.value().emit.nanoseconds(), NanosecondTimeBase);
    if (!target) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            target.error());
    }
    if (target.value() > std::chrono::steady_clock::now()) {
        m_pendingDeadline = target.value();
        m_pendingBuffer = std::move(buffer);
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_pendingDeadline));
    }
    return processProgress(
        emitOutput(context, "scheduled_video", buffer));
}

::media::Result<MediaNodeProcessResult>
MediaVideoOutputSchedulerNode::emitPending(
    MediaGraphExecutionContext& context)
{
    if (!m_pendingDeadline) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "VideoOnly scheduler retained a packet without a deadline"));
    }
    if (std::chrono::steady_clock::now() < *m_pendingDeadline) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_pendingDeadline));
    }
    MediaBufferRef buffer = std::move(m_pendingBuffer);
    m_pendingDeadline.reset();
    return processProgress(
        emitOutput(context, "scheduled_video", buffer));
}

::media::Status MediaVideoOutputSchedulerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaVideoOutputSchedulerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaVideoOutputSchedulerNode::resetState() noexcept
{
    m_configured = false;
    m_requireKeyFrame = false;
    m_startedMedia = false;
    m_maximumStartupWait = MediaRunningTime::fromNanoseconds(0);
    m_transportLead = MediaRunningTime::fromNanoseconds(0);
    m_packetCapacity = 0;
    m_maximumUnitBytes = 0;
    m_byteCapacity = 0;
    m_sourceTimeBase = {};
    m_outputFrameRate = {};
    m_packetTimeBase = {};
    m_packetTimingMode = PacketTimingMode::SourceTimeBase;
    m_startedAt = {};
    m_pendingDeadline.reset();
    m_pendingBuffer.reset();
    m_pacingClock.reset();
}

} // namespace media::ffmpeg::graph
