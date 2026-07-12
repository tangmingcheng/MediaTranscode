#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::int64_t steadyNowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

MediaRtpClockGroupNode::MediaRtpClockGroupNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpClockGroupNode")
{
}

MediaNodeKind MediaRtpClockGroupNode::staticKind() noexcept
{
    return MediaNodeKind::RtpClockGroup;
}

::media::Result<MediaNodeProcessResult> MediaRtpClockGroupNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (!m_initialPublished) {
        m_initialPublished = true;
        auto initial = publish(context, steadyNowNs());
        if (initial) return processProgress();
        if (initial.error().code != ::media::ErrorCode::NotInitialized) {
            return ::media::Result<MediaNodeProcessResult>::failure(initial.error());
        }
    }
    constexpr int MaximumEnvelopesPerProcess = 4;
    constexpr int MaximumPrioritizedInvalidationsPerStream = 2;
    bool processedAny = false;
    int prioritizedVideoInvalidations = 0;
    int prioritizedAudioInvalidations = 0;
    for (int index = 0; index < MaximumEnvelopesPerProcess; ++index) {
        auto invalidation = pendingInvalidation(context);
        if (!invalidation) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                invalidation.error());
        }
        const MediaStreamKind preferred = m_preferAudio
            ? MediaStreamKind::Audio
            : MediaStreamKind::Video;
        const bool invalidationPriorityExhausted = invalidation.value() &&
            (*invalidation.value() == MediaStreamKind::Video
                 ? prioritizedVideoInvalidations
                 : prioritizedAudioInvalidations) >=
                MaximumPrioritizedInvalidationsPerStream;
        const MediaStreamKind first = invalidation.value() &&
                !invalidationPriorityExhausted
            ? *invalidation.value()
            : preferred;
        const MediaStreamKind secondary = first == MediaStreamKind::Video
            ? MediaStreamKind::Audio
            : MediaStreamKind::Video;
        auto processed = processStream(context, first);
        if (!processed) return ::media::Result<MediaNodeProcessResult>::failure(processed.error());
        MediaStreamKind selected = first;
        if (!processed.value()) {
            processed = processStream(context, secondary);
            if (!processed) return ::media::Result<MediaNodeProcessResult>::failure(processed.error());
            selected = secondary;
        }
        if (!processed.value()) break;
        processedAny = true;
        if (invalidation.value() && selected == *invalidation.value()) {
            if (selected == MediaStreamKind::Video) {
                ++prioritizedVideoInvalidations;
            } else {
                ++prioritizedAudioInvalidations;
            }
        }
        m_preferAudio = selected == MediaStreamKind::Video;
    }
    return processedAny ? processProgress() : processWaiting();
}

::media::Status MediaRtpClockGroupNode::configure(MediaGraphExecutionContext& context)
{
    if (m_configured) return ::media::Status::success();
    const MediaNodeOptions* options = nodeOptions(context);
    auto videoRate = requiredPositiveIntNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.video_clock_rate");
    auto audioRate = requiredPositiveIntNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.audio_clock_rate");
    auto timeout = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.sender_report_timeout_ns");
    auto extrapolation = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_extrapolation_ns");
    auto skew = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_inter_stream_skew_ns");
    auto residual = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_sender_clock_residual_ns");
    auto videoCnameTimeout = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.video_cname_timeout_ns");
    auto audioCnameTimeout = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.audio_cname_timeout_ns");
    auto maximumRateError = requiredPositiveIntNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_sender_clock_rate_error_ppm");
    if (!videoRate || !audioRate || !timeout || !extrapolation || !skew || !residual ||
        !videoCnameTimeout || !audioCnameTimeout || !maximumRateError) {
        if (!videoRate) return ::media::Status::failure(videoRate.error());
        if (!audioRate) return ::media::Status::failure(audioRate.error());
        if (!timeout) return ::media::Status::failure(timeout.error());
        if (!extrapolation) return ::media::Status::failure(extrapolation.error());
        if (!skew) return ::media::Status::failure(skew.error());
        if (!residual) return ::media::Status::failure(residual.error());
        if (!videoCnameTimeout) return ::media::Status::failure(videoCnameTimeout.error());
        if (!audioCnameTimeout) return ::media::Status::failure(audioCnameTimeout.error());
        return ::media::Status::failure(maximumRateError.error());
    }
    m_videoConfig = {videoRate.value(), timeout.value(), extrapolation.value(),
                     residual.value(), maximumRateError.value()};
    m_audioConfig = {audioRate.value(), timeout.value(), extrapolation.value(),
                     residual.value(), maximumRateError.value()};
    auto validator = MediaRtpClockGroupValidator::create(
        {timeout.value(), extrapolation.value(), skew.value(),
         videoCnameTimeout.value(), audioCnameTimeout.value()});
    if (!validator) return ::media::Status::failure(validator.error());
    m_validator = std::make_unique<MediaRtpClockGroupValidator>(std::move(validator).value());
    m_configured = true;
    return ::media::Status::success();
}

::media::Result<bool> MediaRtpClockGroupNode::processStream(
    MediaGraphExecutionContext& context,
    MediaStreamKind streamKind)
{
    StreamPending& pending = streamKind == MediaStreamKind::Video
        ? m_videoPending
        : m_audioPending;
    const char* eventPort = streamKind == MediaStreamKind::Video
        ? "video_event"
        : "audio_event";
    const char* clockPort = streamKind == MediaStreamKind::Video
        ? "video_clock"
        : "audio_clock";
    if (auto status = fillPending(context, eventPort, pending.event); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (auto status = fillPending(context, clockPort, pending.clock); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (!pending.event && !pending.clock) {
        return ::media::Result<bool>::success(false);
    }
    const auto* eventEnvelope = pending.event
        ? dynamic_cast<const MediaRtpIngressEventBuffer*>(pending.event.get())
        : nullptr;
    const auto* clockEnvelope = pending.clock
        ? dynamic_cast<const MediaRtpIngressEventBuffer*>(pending.clock.get())
        : nullptr;
    if ((pending.event && !eventEnvelope) || (pending.clock && !clockEnvelope)) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode requires ordered RTP ingress event buffers"));
    }
    if (eventEnvelope && clockEnvelope &&
        eventEnvelope->sequence() == clockEnvelope->sequence()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode requires unique per-stream ingress sequence"));
    }
    const bool selectedClock = clockEnvelope &&
        (!eventEnvelope || clockEnvelope->sequence() < eventEnvelope->sequence());
    MediaBufferRef selected = selectedClock
        ? std::move(pending.clock)
        : std::move(pending.event);
    const auto* event = dynamic_cast<const MediaRtpIngressEventBuffer*>(selected.get());
    if (!event) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode requires RTP ingress event buffers"));
    }
    if (event->sequence() == 0 ||
        (pending.lastProcessedSequence &&
         event->sequence() <= *pending.lastProcessedSequence)) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode requires strictly ordered ingress sequence"));
    }
    pending.lastProcessedSequence = event->sequence();
    ::media::Status status = ::media::Status::success();
    std::int64_t observationTimeNs = steadyNowNs();
    if (selectedClock) {
        status = processClock(*event, streamKind);
    } else if (event->kind() == MediaRtpIngressEventKind::ClockObservation &&
               event->clockObservation()) {
        observationTimeNs = event->clockObservation()->observedAtNs;
        if (observationTimeNs < 0) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRtpClockGroupNode clock observation time must be non-negative"));
        }
    } else if (event->kind() == MediaRtpIngressEventKind::ClockInvalidation &&
               event->clockInvalidation()) {
        auto& minimumGeneration = streamKind == MediaStreamKind::Video
            ? m_minimumVideoGeneration
            : m_minimumAudioGeneration;
        const std::uint64_t invalidatedGeneration = event->clockInvalidation()->generation;
        minimumGeneration = minimumGeneration
            ? std::max(*minimumGeneration, invalidatedGeneration)
            : invalidatedGeneration;
        m_validator->invalidate();
        m_videoMapper.reset();
        m_audioMapper.reset();
        m_videoGeneration.reset();
        m_audioGeneration.reset();
    } else if (event->kind() == MediaRtpIngressEventKind::Discontinuity &&
               event->discontinuity() && event->discontinuityGeneration()) {
        auto& minimumGeneration = streamKind == MediaStreamKind::Video
            ? m_minimumVideoGeneration
            : m_minimumAudioGeneration;
        const std::uint64_t invalidatedGeneration = *event->discontinuityGeneration();
        minimumGeneration = minimumGeneration
            ? std::max(*minimumGeneration, invalidatedGeneration)
            : invalidatedGeneration;
        m_validator->invalidate();
        m_videoMapper.reset();
        m_audioMapper.reset();
        m_videoGeneration.reset();
        m_audioGeneration.reset();
    } else {
        status = ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode event port requires discontinuity"));
    }
    if (!status) {
        m_validator->invalidate();
    }
    auto published = publish(context, observationTimeNs);
    if (!published && published.error().code != ::media::ErrorCode::NotInitialized) {
        return ::media::Result<bool>::failure(published.error());
    }
    return ::media::Result<bool>::success(true);
}

::media::Status MediaRtpClockGroupNode::fillPending(
    MediaGraphExecutionContext& context,
    const char* portName,
    MediaBufferRef& pending)
{
    if (pending) return ::media::Status::success();
    auto input = tryPopInputOptional(context, portName);
    if (!input) return ::media::Status::failure(input.error());
    if (input.value()) pending = std::move(*input.value());
    return ::media::Status::success();
}

::media::Result<std::optional<MediaStreamKind>>
MediaRtpClockGroupNode::pendingInvalidation(MediaGraphExecutionContext& context)
{
    if (auto status = fillPending(context, "video_event", m_videoPending.event);
        !status) {
        return ::media::Result<std::optional<MediaStreamKind>>::failure(
            status.error());
    }
    if (auto status = fillPending(context, "audio_event", m_audioPending.event);
        !status) {
        return ::media::Result<std::optional<MediaStreamKind>>::failure(
            status.error());
    }
    const auto invalidates = [](const MediaBufferRef& buffer) {
        const auto* event = buffer
            ? dynamic_cast<const MediaRtpIngressEventBuffer*>(buffer.get())
            : nullptr;
        return event &&
            (event->kind() == MediaRtpIngressEventKind::ClockInvalidation ||
             event->kind() == MediaRtpIngressEventKind::Discontinuity);
    };
    const bool video = invalidates(m_videoPending.event);
    const bool audio = invalidates(m_audioPending.event);
    if (!video && !audio) {
        return ::media::Result<std::optional<MediaStreamKind>>::success(
            std::nullopt);
    }
    return ::media::Result<std::optional<MediaStreamKind>>::success(
        video && audio
            ? std::optional<MediaStreamKind>(m_preferAudio
                  ? MediaStreamKind::Audio
                  : MediaStreamKind::Video)
            : std::optional<MediaStreamKind>(video
                  ? MediaStreamKind::Video
                  : MediaStreamKind::Audio));
}

::media::Status MediaRtpClockGroupNode::processClock(
    const MediaRtpIngressEventBuffer& event,
    MediaStreamKind streamKind)
{
    if (event.kind() != MediaRtpIngressEventKind::ClockEvidence || !event.clockEvidence()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode clock port requires clock evidence"));
    }
    const MediaRtcpClockEvidence& evidence = *event.clockEvidence();
    const auto& minimumGeneration = streamKind == MediaStreamKind::Video
        ? m_minimumVideoGeneration
        : m_minimumAudioGeneration;
    if (minimumGeneration && evidence.generation < *minimumGeneration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode rejects clock evidence from an invalidated generation"));
    }
    auto& mapper = streamKind == MediaStreamKind::Video ? m_videoMapper : m_audioMapper;
    auto& generation = streamKind == MediaStreamKind::Video ? m_videoGeneration : m_audioGeneration;
    const auto& config = streamKind == MediaStreamKind::Video ? m_videoConfig : m_audioConfig;
    if (!mapper || !generation || *generation != evidence.generation) {
        auto created = MediaRtpSourceClockMapper::create(config, evidence.generation);
        if (!created) return ::media::Status::failure(created.error());
        mapper = std::make_unique<MediaRtpSourceClockMapper>(std::move(created).value());
        generation = evidence.generation;
    }
    auto observed = mapper->observeSenderReport(evidence);
    if (!observed) return observed;
    auto calibration = mapper->calibration(evidence.senderReportObservedAtNs);
    if (!calibration) return ::media::Status::failure(calibration.error());
    return m_validator->observe(streamKind, evidence, std::move(calibration).value());
}

::media::Status MediaRtpClockGroupNode::publish(
    MediaGraphExecutionContext& context,
    std::int64_t observedAtNs)
{
    if (!context.findOutputChannel(nodeId(), "clock_group")) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("RTP clock group output has no downstream consumer yet"));
    }
    return emitOutput(context,
                      "clock_group",
                      makeMediaBufferRef<MediaRtpClockGroupBuffer>(m_validator->snapshot(observedAtNs)));
}

::media::Status MediaRtpClockGroupNode::stop(MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpClockGroupNode::abort(MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaRtpClockGroupNode::resetState() noexcept
{
    m_videoMapper.reset();
    m_audioMapper.reset();
    m_validator.reset();
    m_videoGeneration.reset();
    m_audioGeneration.reset();
    m_minimumVideoGeneration.reset();
    m_minimumAudioGeneration.reset();
    m_videoPending = {};
    m_audioPending = {};
    m_preferAudio = false;
    m_configured = false;
    m_initialPublished = false;
}

} // namespace media::ffmpeg::graph
