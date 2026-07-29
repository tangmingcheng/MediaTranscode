#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/nodes/sync/MediaAvSyncSourceClockModeNodeOptionCodec.h"

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
MediaRealtimeAvSyncNodeConfigurator::configureDemuxPacketClockBinder(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream,
    const MediaAvSyncGroupKey& syncGroup,
    const MediaDemuxTimestampInputClockAssemblyPlan& plan)
{
    const bool video = stream == MediaStreamKind::Video;
    const MediaRational& timeBase =
        video ? plan.videoTimeBase : plan.audioTimeBase;
    if ((!video && stream != MediaStreamKind::Audio) ||
        !syncGroup.valid() ||
        plan.videoTimeBase.num <= 0 || plan.videoTimeBase.den <= 0 ||
        plan.audioTimeBase.num <= 0 || plan.audioTimeBase.den <= 0 ||
        plan.firstWindowMaximumSkew <=
            MediaRunningTime::fromNanoseconds(0) ||
        plan.timestampRegressionLimit <=
            MediaRunningTime::fromNanoseconds(0) ||
        plan.discontinuityThreshold <= plan.timestampRegressionLimit ||
        plan.initialGeneration == 0 ||
        plan.videoSourceIdentity.empty() ||
        plan.audioSourceIdentity.empty() ||
        plan.videoSourceIdentity == plan.audioSourceIdentity) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux clock binder requires the complete planner clock policy"));
    }
    for (auto [key, value] : {
             std::pair{"demux_clock_binder.stream",
                       std::string(video ? "video" : "audio")},
             std::pair{"demux_clock_binder.sync_group",
                       syncGroup.value()},
             std::pair{"demux_clock_binder.time_base_num",
                       std::to_string(timeBase.num)},
             std::pair{"demux_clock_binder.time_base_den",
                       std::to_string(timeBase.den)},
             std::pair{"demux_clock_binder.video_time_base_num",
                       std::to_string(plan.videoTimeBase.num)},
             std::pair{"demux_clock_binder.video_time_base_den",
                       std::to_string(plan.videoTimeBase.den)},
             std::pair{"demux_clock_binder.audio_time_base_num",
                       std::to_string(plan.audioTimeBase.num)},
             std::pair{"demux_clock_binder.audio_time_base_den",
                       std::to_string(plan.audioTimeBase.den)},
             std::pair{"demux_clock_binder.first_window_maximum_skew_ns",
                       std::to_string(
                           plan.firstWindowMaximumSkew.nanoseconds())},
             std::pair{"demux_clock_binder.timestamp_regression_limit_ns",
                       std::to_string(
                           plan.timestampRegressionLimit.nanoseconds())},
             std::pair{"demux_clock_binder.discontinuity_threshold_ns",
                       std::to_string(
                           plan.discontinuityThreshold.nanoseconds())},
              std::pair{"demux_clock_binder.initial_generation",
                        std::to_string(plan.initialGeneration)},
              std::pair{"demux_clock_binder.video_source_identity",
                        plan.videoSourceIdentity},
              std::pair{"demux_clock_binder.audio_source_identity",
                        plan.audioSourceIdentity},
              std::pair{"demux_clock_binder.canonical_target_epoch_ns",
                        std::to_string(
                            plan.canonicalTargetEpoch.nanoseconds())}}) {
        if (auto status = setOption(
                graph, node, key, std::move(value)); !status) {
            return status;
        }
    }
    return ::media::Result<void>::success();
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureLockedPacketGate(
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
            graph, node, "locked_packet_gate.stream",
            isVideo ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "locked_packet_gate.acquiring_timeout_ns",
            std::to_string(acquiringTimeout.nanoseconds())); !status) {
        return status;
    }
    if (plan.assembly.generationPolicy !=
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange ||
        plan.assembly.initialGeneration == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Locked packet gate requires the exact planned initial generation policy"));
    }
    if (auto status = setOption(
            graph, node, "locked_packet_gate.initial_generation",
            std::to_string(plan.assembly.initialGeneration)); !status) {
        return status;
    }
    if (auto status = setOption(
            graph, node, "locked_packet_gate.initial_generation_policy",
            "first_locked_only_fail_on_change"); !status) {
        return status;
    }
    return setOption(
        graph, node, "locked_packet_gate.sync_group",
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
    if (const auto* packet =
            std::get_if<MediaPacketDurationPlan>(
                &assembly.audio.duration)) {
        if (!packet->requirePositiveDuration ||
            !plan.planningFacts.inputAudioSampleRate ||
            *plan.planningFacts.inputAudioSampleRate <= 0) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical packet-duration audio requires its planned sample rate"));
        }
        if (auto status = setOption(
                graph, node, "canonical_input.duration_source",
                "packet"); !status) {
            return status;
        }
        return setOption(
            graph, node, "canonical_input.audio_sample_rate",
            std::to_string(
                *plan.planningFacts.inputAudioSampleRate));
    }
    return ::media::Result<void>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Canonical audio input requires planner-provided sample duration"));
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
        startup.allowDegradedClock && plan.synchronization.sourceClockMode &&
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
    auto sourceClockMode =
        MediaAvSyncSourceClockModeNodeOptionCodec::encode(
            *plan.synchronization.sourceClockMode);
    if (!sourceClockMode) {
        return ::media::Result<void>::failure(sourceClockMode.error());
    }
    return setOption(
        graph, node, "av_startup.source_clock_mode",
        std::move(sourceClockMode).value());
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

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureBoundReleaseExtractor(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    return setOption(
        graph,
        node,
        "av_bound_release_extractor.sync_group",
        plan.groupKey.value());
}

} // namespace media::ffmpeg::graph
