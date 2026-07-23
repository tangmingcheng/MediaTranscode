#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId node,
                                std::string_view key,
                                std::string value)
{
    return MediaRealtimeAvSyncInputGraphSupport::setOption(
        graph, node, key, std::move(value));
}

} // namespace

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureRtpPacketClockBinder(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const bool isVideo = stream == MediaStreamKind::Video;
    const std::size_t acquiringCapacity = isVideo
        ? plan.assembly.video.acquiringCapacity
        : plan.assembly.audio.acquiringCapacity;
    const MediaRunningTime acquiringTimeout = isVideo
        ? plan.assembly.video.acquiringTimeout
        : plan.assembly.audio.acquiringTimeout;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.stream",
            isVideo ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.acquiring_capacity",
            std::to_string(acquiringCapacity)); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.acquiring_timeout_ns",
            std::to_string(acquiringTimeout.nanoseconds()));
        !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.sync_group",
            plan.groupKey.value()); !status) return status;
    if (!isVideo) return ::media::Result<void>::success();

    const auto* duration = std::get_if<MediaRtpTimestampDeltaDurationPlan>(
        &plan.assembly.video.duration);
    if (!duration || duration->clockRate <= 0 ||
        duration->terminalPolicy !=
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video binder requires the complete planned duration policy"));
    }
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.duration_clock_rate",
            std::to_string(duration->clockRate)); !status) return status;
    return setOption(
        graph, node, "rtp_clock_binder.terminal_duration_policy",
        "repeat_last_observed_positive_delta");
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureInitialLockedPacketGate(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const bool isVideo = stream == MediaStreamKind::Video;
    const MediaRunningTime acquiringTimeout = isVideo
        ? plan.assembly.video.acquiringTimeout
        : plan.assembly.audio.acquiringTimeout;
    if (auto status = setOption(
            graph, node, "initial_locked_gate.stream",
            isVideo ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "initial_locked_gate.acquiring_timeout_ns",
            std::to_string(acquiringTimeout.nanoseconds())); !status) {
        return status;
    }
    return setOption(
        graph, node, "initial_locked_gate.sync_group",
        plan.groupKey.value());
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureCanonicalInput(
    MediaGraph& graph,
    MediaNodeId node,
    MediaScheduledStream stream,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& assembly = plan.assembly;
    const bool video = stream == MediaScheduledStream::Video;
    if (auto status = setOption(
            graph, node, "canonical_input.stream",
            video ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "canonical_input.source_identity",
            video ? assembly.video.sourceIdentity
                  : assembly.audio.sourceIdentity); !status) return status;
    const MediaDecodeOrderMode order = video
        ? assembly.video.decodeOrder : assembly.audio.decodeOrder;
    if (auto status = setOption(
            graph, node, "canonical_input.decode_order",
            order == MediaDecodeOrderMode::ReorderedRequiresDecodeTime
                ? "reordered" : "presentation"); !status) return status;
    if (video) {
        return setOption(
            graph, node, "canonical_input.duration_source", "packet");
    }
    if (const auto* samples =
            std::get_if<MediaPlannedAudioSamplesDurationPlan>(
                &assembly.audio.duration)) {
        if (auto status = setOption(
                graph, node, "canonical_input.duration_source",
                "audio_samples"); !status) return status;
        if (auto status = setOption(
                graph, node, "canonical_input.audio_sample_count",
                std::to_string(samples->samplesPerAccessUnit));
            !status) return status;
        return setOption(
            graph, node, "canonical_input.audio_sample_rate",
            std::to_string(samples->sampleRate));
    }
    if (!std::holds_alternative<MediaPacketDurationPlan>(
            assembly.audio.duration)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio input requires a planned duration source"));
    }
    if (!plan.planningFacts.inputAudioSampleRate ||
        *plan.planningFacts.inputAudioSampleRate <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical MPEG-TS audio requires the planned input sample rate"));
    }
    if (auto status = setOption(
            graph, node, "canonical_input.duration_source", "packet");
        !status) return status;
    return setOption(
        graph, node, "canonical_input.audio_sample_rate",
        std::to_string(*plan.planningFacts.inputAudioSampleRate));
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureStartupCoordinator(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& startup = plan.synchronization.startup;
    const bool complete = startup.requireVideoKeyFrame &&
        startup.trimAudioToCommonStart && startup.maximumWaitNs &&
        startup.prerollNs && startup.keyFrameWaitNs &&
        startup.maximumAudioTrimNs && startup.maximumInitialSkewNs &&
        startup.maximumGapNs && startup.outputLeadNs &&
        startup.videoCapacity && startup.audioCapacity &&
        startup.videoByteCapacity && startup.audioByteCapacity &&
        startup.maximumVideoUnitBytes && startup.maximumAudioUnitBytes &&
        startup.videoIdentity && startup.audioIdentity &&
        startup.allowDegradedClock && plan.synchronization.topology &&
        plan.synchronization.audioServo.outputSampleRate &&
        *plan.synchronization.audioServo.outputSampleRate > 0;
    if (!complete) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup coordinator requires a complete planner product"));
    }
    const auto setBool = [&](const char* key, bool value) {
        return setOption(graph, node, key, value ? "1" : "0");
    };
    const auto setTime = [&](const char* key, MediaRunningTime value) {
        return setOption(
            graph, node, key, std::to_string(value.nanoseconds()));
    };
    if (auto status = setBool(
            "av_startup.require_video_key_frame",
            *startup.requireVideoKeyFrame); !status) return status;
    if (auto status = setBool(
            "av_startup.trim_audio_to_common_start",
            *startup.trimAudioToCommonStart); !status) return status;
    if (auto status = setBool(
            "av_startup.allow_degraded_clock",
            *startup.allowDegradedClock); !status) return status;
    for (const auto [key, value] : {
             std::pair{"av_startup.maximum_wait_ns", *startup.maximumWaitNs},
             std::pair{"av_startup.preroll_ns", *startup.prerollNs},
             std::pair{"av_startup.key_frame_wait_ns", *startup.keyFrameWaitNs},
             std::pair{"av_startup.maximum_audio_trim_ns", *startup.maximumAudioTrimNs},
             std::pair{"av_startup.maximum_initial_skew_ns", *startup.maximumInitialSkewNs},
             std::pair{"av_startup.maximum_gap_ns", *startup.maximumGapNs},
             std::pair{"av_startup.output_lead_ns", *startup.outputLeadNs}}) {
        if (auto status = setTime(key, value); !status) return status;
    }
    if (auto status = setOption(
            graph, node, "av_startup.output_audio_sample_rate",
            std::to_string(
                *plan.synchronization.audioServo.outputSampleRate));
        !status) return status;
    for (const auto& [key, value] : {
             std::pair{"av_startup.video_capacity", *startup.videoCapacity},
             std::pair{"av_startup.audio_capacity", *startup.audioCapacity},
             std::pair{"av_startup.video_byte_capacity", *startup.videoByteCapacity},
             std::pair{"av_startup.audio_byte_capacity", *startup.audioByteCapacity},
             std::pair{"av_startup.maximum_video_unit_bytes", *startup.maximumVideoUnitBytes},
             std::pair{"av_startup.maximum_audio_unit_bytes", *startup.maximumAudioUnitBytes}}) {
        if (auto status = setOption(
                graph, node, key, std::to_string(value)); !status) return status;
    }
    if (auto status = setOption(
            graph, node, "av_startup.video_identity",
            *startup.videoIdentity); !status) return status;
    if (auto status = setOption(
            graph, node, "av_startup.audio_identity",
            *startup.audioIdentity); !status) return status;
    if (auto status = setOption(
            graph, node, "av_startup.sync_group",
            plan.groupKey.value()); !status) return status;
    return setOption(
        graph, node, "av_startup.topology",
        *plan.synchronization.topology ==
                MediaAvSyncTopology::SeparateRtpToSeparateRtp
            ? "separate_rtp" : "mpegts");
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureStartupClock(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (auto status = setOption(
            graph, node, "av_startup_clock.sync_group",
            plan.groupKey.value()); !status) return status;
    return setOption(
        graph, node, "av_startup_clock.interval_ns",
        std::to_string(plan.assembly.startupClockInterval.nanoseconds()));
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configurePlaybackEpochBinder(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    return setOption(
        graph, node, "playback_epoch_binder.sync_group",
        plan.groupKey.value());
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureActivationSequencer(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (auto status = setOption(
        graph, node, "activated_startup_release_sequencer.sync_group",
        plan.groupKey.value()); !status) return status;
    return setOption(
        graph, node, "activated_startup_release_sequencer.output_lead_ns",
        std::to_string(
            plan.synchronization.startup.outputLeadNs->nanoseconds()));
}

} // namespace media::ffmpeg::graph
