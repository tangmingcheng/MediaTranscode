#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpClockGroupNode::MediaRtpClockGroupNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpClockGroupNode")
{
}

MediaNodeKind MediaRtpClockGroupNode::staticKind() noexcept
{
    return MediaNodeKind::RtpClockGroup;
}

::media::Status MediaRtpClockGroupNode::bindPreparedSession(std::shared_ptr<MediaPreparedRtpSourceSession> source)
{
    if (!source || m_preparedSource || m_configured)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Clock group requires one prepared source before configuration"));
    m_preparedSource = std::move(source);
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaRtpClockGroupNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (!m_initialPublished) {
        m_initialPublished = true;
        auto initial = publish(context, mediaSteadyClockNowNs());
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
        const MediaStreamKind preferred = m_session->hasAudio() && m_preferAudio
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
        if (processed.value() == StreamProcessOutcome::NoInput && m_session->hasAudio()) {
            processed = processStream(context, secondary);
            if (!processed) return ::media::Result<MediaNodeProcessResult>::failure(processed.error());
            selected = secondary;
        }
        if (processed.value() == StreamProcessOutcome::NoInput) break;
        processedAny = true;
        if (processed.value() == StreamProcessOutcome::Invalidation) {
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
    auto memberOption = requiredNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.members");
    if (!memberOption) return ::media::Status::failure(memberOption.error());
    auto members = MediaTranscodeStreamSetCodec::decode(memberOption.value());
    if (!members) return ::media::Status::failure(members.error());
    const auto memberSet = members.value();
    auto timeout = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.sender_report_timeout_ns");
    auto extrapolation = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_extrapolation_ns");
    auto residual = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_sender_clock_residual_ns");
    auto videoCnameTimeout = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.video_cname_timeout_ns");
    auto invalidateOnDegraded = requiredBoolNodeOption(
        options, "MediaRtpClockGroupNode", "rtp_clock_group.invalidate_on_degraded");
    if (!invalidateOnDegraded) return ::media::Status::failure(invalidateOnDegraded.error());
    auto requireMatchingCname = requiredBoolNodeOption(
        options, "MediaRtpClockGroupNode",
        "rtp_clock_group.require_matching_cname");
    auto maximumRateError = requiredPositiveIntNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_sender_clock_rate_error_ppm");
    auto commonEpochPolicyValue = requiredNodeOption(
        options, "MediaRtpClockGroupNode",
        "rtp_clock_group.common_epoch_policy");
    if (!videoRate || !timeout || !extrapolation || !residual ||
        !videoCnameTimeout || !requireMatchingCname ||
        !maximumRateError ||
        !commonEpochPolicyValue) {
        if (!videoRate) return ::media::Status::failure(videoRate.error());
        if (!timeout) return ::media::Status::failure(timeout.error());
        if (!extrapolation) return ::media::Status::failure(extrapolation.error());
        if (!residual) return ::media::Status::failure(residual.error());
        if (!videoCnameTimeout) return ::media::Status::failure(videoCnameTimeout.error());
        if (!requireMatchingCname) {
            return ::media::Status::failure(requireMatchingCname.error());
        }
        if (!maximumRateError) return ::media::Status::failure(maximumRateError.error());
        return ::media::Status::failure(commonEpochPolicyValue.error());
    }
    auto commonEpochPolicy = parseMediaRtpCommonEpochPolicy(
        commonEpochPolicyValue.value());
    if (!commonEpochPolicy) {
        return ::media::Status::failure(commonEpochPolicy.error());
    }
    const MediaRtpSourceClockMapperConfig videoConfig{videoRate.value(), requireMatchingCname.value(),
                     timeout.value(), extrapolation.value(),
                     residual.value(), maximumRateError.value()};
    std::optional<MediaRtpClockGroupAudioConfig> audio;
    std::optional<MediaRtpSourceClockMapperConfig> audioConfig;
    if (memberSet == MediaTranscodeStreamSet::AudioVideo) {
        auto rate = requiredPositiveIntNodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.audio_clock_rate");
        auto cname = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.audio_cname_timeout_ns");
        auto skew = requiredPositiveInt64NodeOption(options, "MediaRtpClockGroupNode", "rtp_clock_group.maximum_inter_stream_clock_offset_skew_ns");
        if (!rate) return ::media::Status::failure(rate.error());
        if (!cname) return ::media::Status::failure(cname.error());
        if (!skew) return ::media::Status::failure(skew.error());
        audioConfig = MediaRtpSourceClockMapperConfig{
            rate.value(), requireMatchingCname.value(), timeout.value(),
            extrapolation.value(), residual.value(), maximumRateError.value()};
        audio = MediaRtpClockGroupAudioConfig{cname.value(), skew.value()};
    } else if (options->has("rtp_clock_group.audio_clock_rate") ||
               options->has("rtp_clock_group.audio_cname_timeout_ns") ||
               options->has("rtp_clock_group.maximum_inter_stream_clock_offset_skew_ns") ||
               context.findInputChannel(nodeId(), "audio_clock") ||
               context.findInputChannel(nodeId(), "audio_event")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video RTP clock group rejects unplanned audio configuration or input"));
    }
    auto session = MediaRtpClockGroupSession::create(
        {videoConfig, audioConfig, {memberSet, timeout.value(), extrapolation.value(),
         videoCnameTimeout.value(), audio, requireMatchingCname.value(),
         commonEpochPolicy.value(), invalidateOnDegraded.value()}});
    if (!session) return ::media::Status::failure(session.error());
    if (m_preparedSource) {
        auto prepared = m_preparedSource->takeClockGroup(
            {videoConfig, audioConfig, {memberSet, timeout.value(), extrapolation.value(),
             videoCnameTimeout.value(), audio, requireMatchingCname.value(),
             commonEpochPolicy.value(), invalidateOnDegraded.value()}});
        if (!prepared) return ::media::Status::failure(prepared.error());
        if (prepared.value().hasAudio() != audioConfig.has_value())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Prepared clock-group members conflict with runtime plan"));
        m_session.emplace(std::move(prepared).value());
    } else m_session.emplace(std::move(session).value());
    m_configured = true;
    return ::media::Status::success();
}

::media::Result<MediaRtpClockGroupNode::StreamProcessOutcome>
MediaRtpClockGroupNode::processStream(
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
        return ::media::Result<StreamProcessOutcome>::failure(status.error());
    }
    if (auto status = fillPending(context, clockPort, pending.clock); !status) {
        return ::media::Result<StreamProcessOutcome>::failure(status.error());
    }
    if (!pending.event && !pending.clock) {
        return ::media::Result<StreamProcessOutcome>::success(
            StreamProcessOutcome::NoInput);
    }
    const auto* eventEnvelope = pending.event
        ? dynamic_cast<const MediaRtpIngressEventBuffer*>(pending.event.get())
        : nullptr;
    const auto* clockEnvelope = pending.clock
        ? dynamic_cast<const MediaRtpIngressEventBuffer*>(pending.clock.get())
        : nullptr;
    if ((pending.event && !eventEnvelope) || (pending.clock && !clockEnvelope)) {
        return ::media::Result<StreamProcessOutcome>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode requires ordered RTP ingress event buffers"));
    }
    if (eventEnvelope && clockEnvelope &&
        eventEnvelope->sequence() == clockEnvelope->sequence()) {
        return ::media::Result<StreamProcessOutcome>::failure(
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
        return ::media::Result<StreamProcessOutcome>::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode requires RTP ingress event buffers"));
    }
    auto consumed = m_session->consume(
        {event->kind(), event->sequence(), event->clockEvidence(), event->discontinuity(),
         event->discontinuityGeneration(), event->clockObservation(), event->clockInvalidation()},
        streamKind, selectedClock, mediaSteadyClockNowNs());
    if (!consumed) return ::media::Result<StreamProcessOutcome>::failure(consumed.error());
    if (selectedClock && event->kind() == MediaRtpIngressEventKind::ClockEvidence && event->clockEvidence()) {
        const auto& evidence = *event->clockEvidence();
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_clock_group_candidate node=" + std::to_string(nodeId().value) +
                " stream=" + std::string(streamKind == MediaStreamKind::Video ? "video" : "audio") +
                " generation=" + std::to_string(evidence.generation) +
                " ssrc=" + std::to_string(evidence.observedMediaSsrc) +
                " ntp_seconds=" + std::to_string(evidence.ntp.seconds) +
                " ntp_fraction=" + std::to_string(evidence.ntp.fraction) +
                " arrival_ns=" + std::to_string(evidence.senderReportObservedAtNs));
        if (!consumed.value().rejected) {
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "rtp_clock_group_evidence stream=" +
                    std::string(streamKind == MediaStreamKind::Video
                        ? "video" : "audio") +
                    " generation=" + std::to_string(evidence.generation) +
                    " ssrc=" + std::to_string(evidence.observedMediaSsrc));
        }
    } else if (!selectedClock && event->kind() == MediaRtpIngressEventKind::ClockInvalidation && event->clockInvalidation()) {
        const auto& invalidation = *event->clockInvalidation();
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_source_unavailable ingress_generation=" + std::to_string(invalidation.generation) +
                " reason=" + std::to_string(static_cast<int>(invalidation.reason)) +
                " observed_at_ns=" + std::to_string(invalidation.observedAtNs));
    }
    if (consumed.value().rejected) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_clock_group_rejected node=" + std::to_string(nodeId().value) +
                " stream=" + std::string(streamKind == MediaStreamKind::Video ? "video" : "audio") +
                " ingress_sequence=" + std::to_string(event->sequence()) +
                " error=" + consumed.value().rejected->describe());
    }
    const auto observationTimeNs = consumed.value().observationTimeNs;
    auto published = publish(context, observationTimeNs);
    if (!published && published.error().code != ::media::ErrorCode::NotInitialized) {
        return ::media::Result<StreamProcessOutcome>::failure(published.error());
    }
    const bool invalidation = !selectedClock &&
        (event->kind() == MediaRtpIngressEventKind::ClockInvalidation ||
         event->kind() == MediaRtpIngressEventKind::Discontinuity);
    return ::media::Result<StreamProcessOutcome>::success(
        invalidation ? StreamProcessOutcome::Invalidation
                     : StreamProcessOutcome::Regular);
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
    if (m_session->hasAudio()) {
        if (auto status = fillPending(context, "audio_event", m_audioPending.event); !status)
            return ::media::Result<std::optional<MediaStreamKind>>::failure(status.error());
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

::media::Status MediaRtpClockGroupNode::publish(
    MediaGraphExecutionContext& context,
    std::int64_t observedAtNs)
{
    if (!context.findOutputChannel(nodeId(), "clock_group")) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("RTP clock group output has no downstream consumer yet"));
    }
    auto observed = m_session->snapshot(observedAtNs);
    if (!observed) return ::media::Status::failure(observed.error());
    MediaRtpClockGroupSnapshot snapshot = std::move(observed).value();
    if (m_preparedSource) {
        if (auto status = m_preparedSource->observeRuntimeClockGroup(snapshot); !status) return status;
    }
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "rtp_clock_group_snapshot members=" +
            std::string(snapshot.members == MediaTranscodeStreamSet::AudioVideo
                ? "audio_video" : "video_only") + " state=" +
            std::to_string(static_cast<int>(snapshot.state)) +
            " generation=" + std::to_string(snapshot.groupGeneration) +
            " invalidated_generation=" + (snapshot.invalidatedGeneration
                ? std::to_string(*snapshot.invalidatedGeneration) : "none") +
            " locked=" + (snapshot.locked ? std::string("1")
                                           : std::string("0")));
    return emitOutput(
        context, "clock_group",
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(std::move(snapshot)));
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
    m_session.reset();
    m_videoPending = {};
    m_audioPending = {};
    m_preferAudio = false;
    m_configured = false;
    m_initialPublished = false;
}

} // namespace media::ffmpeg::graph
