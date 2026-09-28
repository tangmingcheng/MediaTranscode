#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvProtocolOutputPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <optional>
#include <limits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaRealtimeEdgePolicySet> planBoundedEdgePolicies(
    const MediaRealtimeRtpTranscodePlanningDraft& outer,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.startup.videoByteCapacity ||
        !synchronization.startup.audioByteCapacity ||
        !synchronization.startup.videoCapacity ||
        !synchronization.startup.audioCapacity ||
        *synchronization.startup.videoByteCapacity == 0 ||
        *synchronization.startup.audioByteCapacity == 0 ||
        outer.queues.packet == 0 ||
        *synchronization.startup.videoByteCapacity >
            (std::numeric_limits<std::uint64_t>::max)() -
                *synchronization.startup.audioByteCapacity) {
        return ::media::Result<MediaRealtimeEdgePolicySet>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V edge byte capacity is incomplete or not representable"));
    }
    const auto maximumBytes =
        *synchronization.startup.videoByteCapacity +
        *synchronization.startup.audioByteCapacity;
    return MediaRealtimeEdgePolicyPlanner::
        planWithAvStartupRelease(
            outer.queues, maximumBytes, outer.queues.packet,
            *synchronization.startup.videoCapacity,
            *synchronization.startup.audioCapacity);
}

::media::Result<MediaRealtimeAvSyncAssemblyPlan> planAssembly(
    const MediaRealtimeRtpTranscodePlanCore& outer,
    const MediaAudioPipelinePlan& audio,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSyncPlanningFacts& facts)
{
    if (!audio.enabled || !synchronization.sourceClockMode ||
        !synchronization.startup.videoIdentity ||
        synchronization.startup.videoIdentity->empty() ||
        !synchronization.startup.audioIdentity ||
        synchronization.startup.audioIdentity->empty() ||
        !synchronization.startup.videoCapacity ||
        *synchronization.startup.videoCapacity == 0 ||
        !synchronization.startup.audioCapacity ||
        *synchronization.startup.audioCapacity == 0 ||
        !synchronization.startup.maximumWaitNs ||
        *synchronization.startup.maximumWaitNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        !synchronization.audioServo.minimumUpdateIntervalNs ||
        *synchronization.audioServo.minimumUpdateIntervalNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        facts.inputVideoIdentity != synchronization.startup.videoIdentity ||
        facts.inputAudioIdentity != synchronization.startup.audioIdentity) {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V production assembly requires complete startup and source facts"));
    }

    MediaAvSyncInputClockPlan inputClock;
    MediaCanonicalVideoDurationPlan videoDuration;
    MediaCanonicalAudioDurationPlan audioDuration;
    std::uint64_t initialGeneration = MediaFirstLockedSourceGeneration;
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (outer.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            !synchronization.rtpInput || !facts.inputVideoClockRate ||
            *facts.inputVideoClockRate <= 0 || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            (outer.videoPlan.inputCodecName != "h264" &&
             outer.videoPlan.inputCodecName != "hevc")) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaRtpInputClockAssemblyPlan>(
            synchronization.rtpInput->input.commonEpochPolicy);
        videoDuration.emplace<MediaRtpTimestampDeltaDurationPlan>(
            *facts.inputVideoClockRate,
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (outer.inputType != RealtimeInputType::MpegTsUdp ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::MuxedTransportStream ||
            !synchronization.mpegTsInput || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            !facts.inputVideoPacketDuration ||
            facts.inputVideoPacketDuration->packetDuration <= 0 ||
            facts.inputVideoPacketDuration->timeBase.num <= 0 ||
            facts.inputVideoPacketDuration->timeBase.den <= 0 ||
            !facts.inputAudioPacketDuration ||
            facts.inputAudioPacketDuration->packetDuration <= 0 ||
            facts.inputAudioPacketDuration->timeBase.num <= 0 ||
            facts.inputAudioPacketDuration->timeBase.den <= 0) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaMpegTsInputClockAssemblyPlan>();
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (outer.inputType != RealtimeInputType::Url ||
            outer.inputLayout !=
                RealtimeInputStreamLayout::SessionDescribed ||
            !synchronization.demuxTimestampInput ||
            !synchronization.demuxTimestampInput->firstWindowMaximumSkewNs ||
            !synchronization.demuxTimestampInput->discontinuityThresholdNs ||
            !synchronization.demuxTimestampInput->initialGeneration ||
            !synchronization.demuxTimestampInput->canonicalTargetEpochNs ||
            !synchronization.demuxTimestampInput->preparedInput ||
            !synchronization.demuxTimestampInput->preparedEvidence ||
            !synchronization.demuxTimestampInput->videoTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->videoTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->videoTimeBase.den <= 0 ||
            !synchronization.demuxTimestampInput->audioTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->audioTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->audioTimeBase.den <= 0 ||
            !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp production assembly facts are incomplete"));
        }
        const auto& demux = *synchronization.demuxTimestampInput;
        initialGeneration = *demux.initialGeneration;
        inputClock.emplace<MediaDemuxTimestampInputClockAssemblyPlan>(
            MediaDemuxTimestampInputClockAssemblyPlan{
                demux.videoTimeBase,
                demux.audioTimeBase,
                *demux.firstWindowMaximumSkewNs,
                *demux.discontinuityThresholdNs,
                initialGeneration,
                *synchronization.startup.videoIdentity,
                *synchronization.startup.audioIdentity,
                *demux.canonicalTargetEpochNs,
                *demux.preparedInput,
                *demux.preparedEvidence});
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V production input clock mode is unsupported"));
    }

    return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::success(
        MediaRealtimeAvSyncAssemblyPlan{
            std::move(inputClock),
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
            initialGeneration,
            MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
            MediaCanonicalVideoAssemblyPlan{
                *synchronization.startup.videoIdentity,
                std::move(videoDuration),
                MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
                *synchronization.startup.videoCapacity,
                *synchronization.startup.maximumWaitNs},
            MediaCanonicalAudioAssemblyPlan{
                *synchronization.startup.audioIdentity,
                std::move(audioDuration),
                MediaDecodeOrderMode::PresentationOrderNoReorder,
                *synchronization.startup.audioCapacity,
                *synchronization.startup.maximumWaitNs},
            *synchronization.audioServo.minimumUpdateIntervalNs});
}

} // namespace

::media::Result<MediaRealtimeAvSyncRuntimePlan>
MediaRealtimeAvSyncRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlanningDraft& outer,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaRealtimeRtpTranscodeRequest& request,
    MediaAvSyncPlan synchronization,
    MediaRational outputFrameRate,
    const MediaPreparedRealtimeEmissionSet& preparedEmission)
{
    if (!outer.audioPlan) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "Synchronized runtime planning requires an audio pipeline product"));
    }
    MediaAudioPipelinePlan& audio = *outer.audioPlan;
    if (audio.branchMode != MediaBranchMode::TranscodeFrame &&
        audio.branchMode != MediaBranchMode::CopyPacket) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime planning requires copy or frame transcode audio"));
    }
    if (outer.videoPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime planning rejects video packet copy"));
    }
    if (!outer.avSyncComponentBounds) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V runtime requires planner-owned component bounds"));
    }
    auto facts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, audio, *outer.avSyncComponentBounds,
        outer.isolatedAudioInput ? &*outer.isolatedAudioInput : nullptr,
        output, synchronization);
    if (!facts) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            facts.error());
    }
    if (!facts.value().acknowledgementTimeout ||
        !facts.value().terminalDrainWindow ||
        !synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V generation transition timing facts are incomplete"));
    }
    std::optional<MediaAudioCorrectionReachabilityResult> correction;
    if (audio.branchMode == MediaBranchMode::TranscodeFrame) {
        auto selected = MediaAudioCorrectionReachabilityPlanner::plan(
            synchronization, facts.value());
        if (!selected) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                selected.error());
        }
        correction = std::move(selected).value();
        synchronization.audioServo.commandLeadNs = correction->commandLead;
        synchronization.audioServo.compensationWindowNs =
            correction->compensationWindow;
        synchronization.audioServo.frequencyFilterTimeConstantNs =
            correction->frequencyFilterTimeConstant;
        if (auto status = MediaAvSyncPlanValidator::validate(synchronization);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                status.error());
        }
    } else {
        if (auto status =
                MediaAvSyncPlanValidator::validatePolicy(synchronization);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                status.error());
        }
    }
    std::optional<MediaAudioEncoderFifoRetentionPlan> encoderFifoRetention;
    if (audio.branchMode == MediaBranchMode::TranscodeFrame) {
        if (!audio.resolvedOutput || !audio.selectedResampler) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::invalidArgument("audio FIFO requires selected resampler and encoder"));
        }
        auto retention = MediaAudioEncoderFifoRetentionPlan::create(
            *audio.resolvedOutput, audio.selectedResampler->maximumOutputBlockSamples,
            synchronization.audioServo);
        if (!retention) return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(retention.error());
        encoderFifoRetention = std::move(retention).value();
    }
    auto assembly = planAssembly(outer, audio, synchronization, facts.value());
    if (!assembly) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            assembly.error());
    }
    auto edgePolicies = planBoundedEdgePolicies(outer, synchronization);
    if (!edgePolicies) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            edgePolicies.error());
    }

    if (!outer.deployment || (synchronization.rtpOutput && !audio.resolvedOutput)) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V protocol output requires deployment and resolved output audio facts"));
    }
    const MediaAvSyncGroupKey groupKey("realtime.av");
    auto protocol = MediaRealtimeAvProtocolOutputPlanner::plan(
        {groupKey, outer.outputLayout, outer.outputTransport,
         synchronization.rtpOutput, synchronization.projectMpegTsOutput,
         synchronization.startup.outputLeadNs, outputFrameRate,
         facts.value().outputSampleRate, facts.value().protocolBatchSamples,
         edgePolicies.value().synchronizedPacket.bufferPolicy.memoryBudget.maxBytes,
         *outer.deployment, preparedEmission}, output);
    if (!protocol) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(protocol.error());
    }
    auto outputPlan = std::move(protocol).value();
    if (outputPlan.adapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        outer.videoParameters.globalHeader = true;
    }
    auto transition = MediaAvGenerationTransitionPlanner::plan(
        outputPlan.protocolOutput,
        *synchronization.sourceClockMode,
        audio.branchMode,
        outer.videoPlan.filterActive,
        *facts.value().acknowledgementTimeout,
        *facts.value().terminalDrainWindow);
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
          MediaRealtimeAvSourceRuntimePlan{
            std::move(audio),
            std::move(outer.isolatedAudioInput),
            groupKey,
            std::move(synchronization),
            std::move(assembly).value(),
            outer.queues,
            std::move(edgePolicies).value(),
            outer.threadingPolicy,
            outputPlan.activationOutputLead,
            outer.videoPlan.filterActive,
            std::move(transition),
            facts.value().inputAudioSampleRate,
            correction
                ? std::optional<MediaAudioCorrectionReachabilityPlan>(
                      correction->correction)
                : std::nullopt}, *outer.avSyncComponentBounds, facts.value(), outputPlan.adapter, std::move(outputPlan.protocolOutput),
            std::move(outputPlan.datagramTransport), std::move(encoderFifoRetention)});
}

} // namespace media::ffmpeg::graph
