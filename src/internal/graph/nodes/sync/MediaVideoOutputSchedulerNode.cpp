#include "internal/graph/nodes/sync/MediaVideoOutputSchedulerNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaOutputSchedule.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

namespace media::ffmpeg::graph {

MediaVideoOutputSchedulerNode::MediaVideoOutputSchedulerNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaVideoOutputSchedulerNode"),
      m_authority(std::move(authority))
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
    auto transportLead = requiredPositiveInt64NodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.transport_lead_ns");
    auto pacingEnabled = requiredBoolNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.pacing_enabled");
    auto session = requiredNodeOption(
        options, "MediaVideoOutputSchedulerNode",
        "protocol_output.session");
    if (!requireKeyFrame || !maximumWait || !packetCapacity ||
        !maximumUnitBytes || !byteCapacity || !sourceNumerator ||
        !sourceDenominator || !frameRateNumerator ||
        !frameRateDenominator || !packetTimeBaseNumerator ||
        !packetTimeBaseDenominator || !packetTimingMode ||
        !transportLead || !pacingEnabled || !session) {
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
            : !pacingEnabled ? pacingEnabled.error()
            : session.error();
        return ::media::Status::failure(error);
    }
    if (!pacingEnabled.value() || !m_authority ||
        m_authority->streamSet() != MediaTranscodeStreamSet::VideoOnly ||
        m_authority->sessionKey().value() != session.value()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler requires its exact protocol output authority"));
    }
    const auto inputs = context.inputChannels(nodeId());
    const auto outputs = context.outputChannels(nodeId());
    if (inputs.size() != 1 || outputs.size() != 2 ||
        !context.findInputChannel(nodeId(), "video") ||
        !context.findOutputChannel(nodeId(), "activation") ||
        !context.findOutputChannel(nodeId(), "scheduled_video")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler requires one video input and exact activation/scheduled outputs"));
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
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly source timing mode requires the source time base"));
        }
    } else if (packetTimingMode.value() == "output_cadence_time_base") {
        m_packetTimingMode = PacketTimingMode::OutputCadenceTimeBase;
        if (m_packetTimeBase.num != m_outputFrameRate.den ||
            m_packetTimeBase.den != m_outputFrameRate.num) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly cadence timing mode requires the inverse output frame rate"));
        }
    } else {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly scheduler received an unknown packet timing mode"));
    }
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

::media::Result<MediaBufferRef> MediaVideoOutputSchedulerNode::schedule(
    MediaBufferRef media)
{
    const auto& timeBase = media->timeDescriptor().timeBase;
    auto presentation = MediaRunningTime::checkedFromTicks(
        media->pts(), timeBase.num, timeBase.den);
    auto dispatch = MediaRunningTime::checkedFromTicks(
        media->dts(), timeBase.num, timeBase.den);
    if (!presentation || !dispatch || !m_sourceStart || !m_masterRelease) {
        return ::media::Result<MediaBufferRef>::failure(
            !presentation ? presentation.error()
            : !dispatch ? dispatch.error()
            : ::media::ErrorInfo::notInitialized(
                  "VideoOnly scheduler has no active source/master origin"));
    }
    if (m_lastDispatch && dispatch.value() <= *m_lastDispatch) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler rejects non-monotonic decode timestamps"));
    }
    auto presentationOffset =
        presentation.value().checkedSubtract(*m_sourceStart);
    auto dispatchOffset = dispatch.value().checkedSubtract(*m_sourceStart);
    if (!presentationOffset || !dispatchOffset) {
        return ::media::Result<MediaBufferRef>::failure(
            presentationOffset ? dispatchOffset.error()
                               : presentationOffset.error());
    }
    auto presentationOnMaster =
        m_masterRelease->checkedAdd(presentationOffset.value());
    auto dispatchOnMaster =
        m_masterRelease->checkedAdd(dispatchOffset.value());
    if (!presentationOnMaster || !dispatchOnMaster) {
        return ::media::Result<MediaBufferRef>::failure(
            presentationOnMaster ? dispatchOnMaster.error()
                                 : presentationOnMaster.error());
    }
    auto outputSchedule = MediaOutputSchedule::create(
        presentationOnMaster.value(), dispatchOnMaster.value(),
        m_transportLead);
    if (!outputSchedule) {
        return ::media::Result<MediaBufferRef>::failure(
            outputSchedule.error());
    }
    ::media::Result<MediaRunningTime> duration = media->duration() > 0
        ? MediaRunningTime::checkedFromTicks(
              media->duration(), timeBase.num, timeBase.den)
        : MediaRunningTime::checkedFromTicks(
              1, m_outputFrameRate.den, m_outputFrameRate.num);
    if (!duration) {
        return ::media::Result<MediaBufferRef>::failure(duration.error());
    }
    auto scheduled = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            std::move(media), MediaScheduledStream::Video,
            presentation.value(), dispatch.value(),
            outputSchedule.value().presentation,
            outputSchedule.value().dispatch,
            outputSchedule.value().emit,
            duration.value(), 1,
            MediaSourceAccessUnitSequence(m_nextSequence++),
            std::nullopt, std::nullopt,
            MediaVideoSyncDecisionKind::Display});
    if (scheduled) m_lastDispatch = dispatch.value();
    return scheduled;
}

::media::Result<MediaNodeProcessResult>
MediaVideoOutputSchedulerNode::emitPending(
    MediaGraphExecutionContext& context)
{
    if (m_pendingActivation) {
        MediaBufferRef activation = std::move(m_pendingActivation);
        auto emitted = emitOutput(context, "activation", activation);
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    if (!m_pendingScheduled || !m_pendingDeadline) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "VideoOnly scheduler pending state is incomplete"));
    }
    auto now = m_authority->now();
    if (!now) {
        return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    }
    if (now.value() < *m_pendingDeadline) {
        return ::media::Result<MediaNodeProcessResult>::success(
            {MediaNodeProcessState::Waiting,
             m_authority->deadlineWait(*m_pendingDeadline)});
    }
    MediaBufferRef scheduled = std::move(m_pendingScheduled);
    m_pendingDeadline.reset();
    auto emitted = emitOutput(context, "scheduled_video", scheduled);
    return emitted ? processProgress()
                   : processProgress(std::move(emitted));
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
    if (m_pendingActivation || m_pendingScheduled) return emitPending(context);
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
                    "VideoOnly scheduler reached end before activation"));
        }
        auto emitted = emitOutput(context, "scheduled_video", buffer);
        return buffer->isEof() ? processFinished(emitted)
                               : processProgress(emitted);
    }
    if (!FFmpegPacketView::isPacket(buffer) ||
        buffer->streamKind() != MediaStreamKind::Video ||
        buffer->payloadKind() != MediaPayloadKind::Packet ||
        buffer->pts() == invalidMediaTimeValue ||
        buffer->dts() == invalidMediaTimeValue ||
        !buffer->timeDescriptor().timeBase.isKnown()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler requires video packets with explicit PTS/DTS/time base"));
    }
    const auto& timeBase = buffer->timeDescriptor().timeBase;
    if (timeBase.num != m_packetTimeBase.num ||
        timeBase.den != m_packetTimeBase.den) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler packet time base disagrees with its plan"));
    }
    const auto footprint = buffer->payloadFootprintBytes();
    if (!footprint || *footprint > m_maximumUnitBytes ||
        *footprint > m_byteCapacity) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler packet exceeds planner byte bounds"));
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
        auto sourceStart = MediaRunningTime::checkedFromTicks(
            buffer->dts(), timeBase.num, timeBase.den);
        if (!sourceStart) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                sourceStart.error());
        }
        auto activation = m_authority->activate(
            sourceStart.value(), m_transportLead);
        if (!activation) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                activation.error());
        }
        auto facts = m_authority->validateActivation(activation.value());
        if (!facts) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                facts.error());
        }
        m_sourceStart = facts.value().sourceStart;
        m_masterRelease = facts.value().masterRelease;
        m_startedMedia = true;
        m_pendingActivation = std::move(activation).value();
    }
    auto scheduled = schedule(std::move(buffer));
    if (!scheduled) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            scheduled.error());
    }
    const auto* unit = dynamic_cast<const MediaScheduledAccessUnit*>(
        scheduled.value().get());
    if (!unit) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "VideoOnly scheduler failed to materialize scheduled AU"));
    }
    m_pendingDeadline = unit->emitOnMaster();
    m_pendingScheduled = std::move(scheduled).value();
    return emitPending(context);
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
    if (m_authority) m_authority->markAborted();
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
    m_pendingActivation.reset();
    m_pendingScheduled.reset();
    m_sourceStart.reset();
    m_masterRelease.reset();
    m_lastDispatch.reset();
    m_nextSequence = 1;
}

} // namespace media::ffmpeg::graph
