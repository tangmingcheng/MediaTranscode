#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/nodes/sync/MediaAvSyncControlGenerationContract.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"
#include "internal/graph/nodes/sync/MediaAvSyncSourceClockModeNodeOptionCodec.h"

#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

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
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    const bool isVideo = stream == MediaStreamKind::Video;
    if ((stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio) ||
        (!isVideo && !plan.assembly.audio))
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "RTP packet binder requires the planned stream assembly"));
    const std::size_t acquiringCapacity = isVideo
        ? plan.assembly.video.acquiringCapacity
        : plan.assembly.audio->acquiringCapacity;
    const MediaRunningTime acquiringTimeout = isVideo
        ? plan.assembly.video.acquiringTimeout
        : plan.assembly.audio->acquiringTimeout;
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

    if (std::holds_alternative<MediaDecodedVideoAssemblyTiming>(plan.assembly.video.timing)) {
        if (plan.synchronization.startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan ||
            !std::holds_alternative<MediaRtpInputClockAssemblyPlan>(plan.assembly.inputClock) ||
            !plan.synchronization.rtpInput ||
            !plan.synchronization.rtpInput->videoInput.clockRate ||
            *plan.synchronization.rtpInput->videoInput.clockRate <= 0 ||
            !plan.synchronization.rtpInput->videoInput.identity ||
            !plan.synchronization.startup.videoIdentity ||
            plan.assembly.video.sourceIdentity.empty() ||
            plan.assembly.video.sourceIdentity != *plan.synchronization.rtpInput->videoInput.identity ||
            plan.assembly.video.sourceIdentity != *plan.synchronization.startup.videoIdentity)
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                "Decoded RTP startup requires its source identity, readiness and protocol clock rate"));
        if (auto status = setOption(graph, node, "rtp_clock_binder.video_timing_product",
                "source_presentation"); !status) return status;
        if (auto status = setOption(graph, node, "rtp_clock_binder.video_clock_rate",
                std::to_string(*plan.synchronization.rtpInput->videoInput.clockRate)); !status) return status;
        return setOption(graph, node, "rtp_clock_binder.source_identity",
            plan.assembly.video.sourceIdentity);
    }
    const auto* canonical = std::get_if<MediaCanonicalVideoAssemblyTiming>(
        &plan.assembly.video.timing);
    const auto* duration = canonical ? std::get_if<MediaRtpTimestampDeltaDurationPlan>(
        &canonical->duration) : nullptr;
    if (plan.synchronization.startup.videoReadiness != MediaVideoStartupReadiness::IntervalCoverage ||
        !duration || duration->clockRate <= 0 ||
        duration->terminalPolicy !=
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video binder requires the complete planned duration policy"));
    }
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.video_timing_product",
            "canonical_interval"); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.video_clock_rate",
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
    if (stream != MediaStreamKind::Video &&
        stream != MediaStreamKind::Audio) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux clock binder requires video or audio stream"));
    }
    const MediaScheduledStream scheduled =
        stream == MediaStreamKind::Video
        ? MediaScheduledStream::Video
        : MediaScheduledStream::Audio;
    auto applied = MediaDemuxPacketClockBinderNodePlanCodec::apply(
        graph, node, scheduled, syncGroup, plan);
    return applied
        ? ::media::Result<void>::success()
        : ::media::Result<void>::failure(applied.error());
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureLockedPacketGate(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    const bool isVideo = stream == MediaStreamKind::Video;
    if ((stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio) ||
        (!isVideo && !plan.assembly.audio))
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Locked packet gate requires the planned stream assembly"));
    const MediaRunningTime acquiringTimeout = isVideo
        ? plan.assembly.video.acquiringTimeout
        : plan.assembly.audio->acquiringTimeout;
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
    if (!plan.synchronization.controlGenerationPolicy) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::notInitialized(
                "Locked packet gate requires planner-selected control generation policy"));
    }
    if (auto status = setOption(
            graph, node,
            "locked_packet_gate.control_generation_policy",
            std::string(mediaControlGenerationPolicyOption(
                *plan.synchronization.controlGenerationPolicy)));
        !status) {
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
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    const auto& assembly = plan.assembly;
    const bool video = stream == MediaScheduledStream::Video;
    if (!video && !assembly.audio)
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Canonical audio input requires the planned audio assembly"));
    if (!plan.synchronization.sourceClockMode) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::notInitialized(
                "Canonical input requires planner-selected source clock mode"));
    }
    const bool mappedDemuxTiming =
        *plan.synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::DemuxTimestamps;
    if (!plan.synchronization.controlGenerationPolicy) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::notInitialized(
                "Canonical input requires planner-selected control generation policy"));
    }
    if (assembly.initialGeneration == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires planned initial generation"));
    }
    if (auto status = setOption(
            graph, node, "canonical_input.initial_generation",
            std::to_string(assembly.initialGeneration)); !status) {
        return status;
    }
    if (auto status = setOption(
            graph, node, "canonical_input.control_generation_policy",
            std::string(mediaControlGenerationPolicyOption(
                *plan.synchronization.controlGenerationPolicy)));
        !status) {
        return status;
    }
    if (auto status = setOption(
            graph, node, "canonical_input.sync_group",
            plan.groupKey.value()); !status) {
        return status;
    }
    if (auto status = setOption(
            graph, node, "canonical_input.stream",
            video ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "canonical_input.source_identity",
            video ? assembly.video.sourceIdentity
                  : assembly.audio->sourceIdentity); !status) return status;
    const auto* canonicalVideo = std::get_if<MediaCanonicalVideoAssemblyTiming>(
        &assembly.video.timing);
    if (video && !canonicalVideo)
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Canonical input rejects a decoded presentation assembly"));
    const MediaDecodeOrderMode order = video
        ? canonicalVideo->decodeOrder : assembly.audio->decodeOrder;
    if (auto status = setOption(
            graph, node, "canonical_input.decode_order",
            order == MediaDecodeOrderMode::ReorderedRequiresDecodeTime
                ? "reordered" : "presentation"); !status) return status;
    if (video) {
        return setOption(
            graph, node, "canonical_input.duration_source",
            mappedDemuxTiming
                ? "mapped_source_timing"
                : "raw_packet_time_base");
    }
    if (const auto* samples =
            std::get_if<MediaPlannedAudioSamplesDurationPlan>(
                &assembly.audio->duration)) {
        if (auto status = setOption(
                graph, node, "canonical_input.duration_source",
                "planned_audio_samples"); !status) return status;
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
                &assembly.audio->duration)) {
        if (!packet->requirePositiveDuration ||
            !plan.sourceTiming.inputAudioSampleRate ||
            *plan.sourceTiming.inputAudioSampleRate <= 0) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical packet-duration audio requires its planned sample rate"));
        }
        if (auto status = setOption(
                graph, node, "canonical_input.duration_source",
                mappedDemuxTiming
                    ? "mapped_source_timing"
                    : "raw_packet_time_base"); !status) {
            return status;
        }
        return setOption(
            graph, node, "canonical_input.audio_sample_rate",
            std::to_string(
                *plan.sourceTiming.inputAudioSampleRate));
    }
    return ::media::Result<void>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Canonical audio input requires planner-provided sample duration"));
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureStartupCoordinator(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    if (auto status = MediaAvSyncPlanValidator::validateRuntime(plan.synchronization); !status)
        return ::media::Result<void>::failure(status.error());
    const auto& startup = plan.synchronization.startup;
    const bool hasAudio = plan.synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    const bool complete = (hasAudio || plan.synchronization.members == MediaTranscodeStreamSet::VideoOnly) &&
        startup.videoReadiness && startup.requireVideoKeyFrame &&
        startup.trimAudioToCommonStart && startup.maximumWaitNs &&
        startup.prerollNs && startup.keyFrameWaitNs &&
        startup.maximumGapNs && startup.outputLeadNs &&
        startup.videoCapacity && startup.videoByteCapacity &&
        startup.maximumVideoUnitBytes && startup.videoIdentity &&
        startup.allowDegradedClock && plan.synchronization.sourceClockMode &&
        (hasAudio ? (startup.maximumAudioTrimNs && startup.maximumInitialSkewNs &&
            startup.audioCapacity && startup.audioByteCapacity && startup.maximumAudioUnitBytes &&
            startup.audioIdentity && plan.synchronization.audioServo.outputSampleRate &&
            *plan.synchronization.audioServo.outputSampleRate > 0)
            : (!startup.maximumAudioTrimNs && !startup.audioCapacity && !startup.audioByteCapacity &&
               !startup.maximumAudioUnitBytes && !startup.audioIdentity &&
               !plan.synchronization.audioServo.outputSampleRate));
    if (!complete) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup coordinator requires a complete planner product"));
    }
    auto readiness = encodeMediaVideoStartupReadiness(*startup.videoReadiness);
    if (!readiness) return ::media::Result<void>::failure(readiness.error());
    if (auto status = setOption(graph, node, "av_startup.video_readiness",
            std::string(readiness.value())); !status) return status;
    auto encodedMembers = MediaTranscodeStreamSetCodec::encode(*plan.synchronization.members);
    if (!encodedMembers) return ::media::Result<void>::failure(encodedMembers.error());
    if (auto status = setOption(graph, node, "av_startup.members",
            std::string(encodedMembers.value())); !status) return status;
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
            "av_startup.allow_degraded_clock",
            *startup.allowDegradedClock); !status) return status;
    for (const auto [key, value] : {
             std::pair{"av_startup.maximum_wait_ns", *startup.maximumWaitNs},
             std::pair{"av_startup.preroll_ns", *startup.prerollNs},
             std::pair{"av_startup.key_frame_wait_ns", *startup.keyFrameWaitNs},
             std::pair{"av_startup.maximum_gap_ns", *startup.maximumGapNs},
             std::pair{"av_startup.output_lead_ns", *startup.outputLeadNs}}) {
        if (auto status = setTime(key, value); !status) return status;
    }
    if (hasAudio) {
        if (auto status = setBool("av_startup.trim_audio_to_common_start",
                *startup.trimAudioToCommonStart); !status) return status;
        if (auto status = setTime("av_startup.maximum_audio_trim_ns",
                *startup.maximumAudioTrimNs); !status) return status;
        if (auto status = setTime("av_startup.maximum_initial_skew_ns",
                *startup.maximumInitialSkewNs); !status) return status;
        if (auto status = setOption(
                graph, node, "av_startup.output_audio_sample_rate",
                std::to_string(
                    *plan.synchronization.audioServo.outputSampleRate));
            !status) return status;
        for (const auto& [key, value] : {
                 std::pair{"av_startup.audio_capacity", *startup.audioCapacity},
                 std::pair{"av_startup.audio_byte_capacity", *startup.audioByteCapacity},
                 std::pair{"av_startup.maximum_audio_unit_bytes", *startup.maximumAudioUnitBytes}}) {
            if (auto status = setOption(
                    graph, node, key, std::to_string(value)); !status) return status;
        }
        if (auto status = setOption(
                graph, node, "av_startup.audio_identity",
                *startup.audioIdentity); !status) return status;
    }
    for (const auto& [key, value] : {
             std::pair{"av_startup.video_capacity", *startup.videoCapacity},
             std::pair{"av_startup.video_byte_capacity", *startup.videoByteCapacity},
             std::pair{"av_startup.maximum_video_unit_bytes", *startup.maximumVideoUnitBytes}}) {
        if (auto status = setOption(graph, node, key, std::to_string(value)); !status) return status;
    }
    if (auto status = setOption(graph, node, "av_startup.video_identity",
            *startup.videoIdentity); !status) return status;
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
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    return setOption(graph, node, "av_startup_clock.sync_group", plan.groupKey.value());
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configurePlaybackEpochBinder(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    return setOption(
        graph, node, "playback_epoch_binder.sync_group",
        plan.groupKey.value());
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureActivationSequencer(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    if (auto status = setOption(
        graph, node, "activated_startup_release_sequencer.sync_group",
        plan.groupKey.value()); !status) return status;
    if (plan.activationOutputLead.nanoseconds() <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Activated startup release sequencer requires planner-owned output lead"));
    }
    return setOption(
        graph, node, "activated_startup_release_sequencer.output_lead_ns",
        std::to_string(plan.activationOutputLead.nanoseconds()));
}

::media::Result<void>
MediaRealtimeAvSyncNodeConfigurator::configureBoundReleaseExtractor(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    if (auto status = setOption(
        graph,
        node,
        "av_bound_release_extractor.sync_group",
        plan.groupKey.value()); !status) {
        return status;
    }
    return setOption(
        graph,
        node,
        "av_bound_release_extractor.audio_branch_mode",
        mediaBranchModeName(plan.audioPipeline.branchMode));
}

} // namespace media::ffmpeg::graph
