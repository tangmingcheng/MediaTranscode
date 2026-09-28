#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlanner.h"

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


} // namespace

::media::Result<MediaRealtimeAvSyncRuntimePlan>
MediaRealtimeAvSyncRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlanningDraft& outer,
    MediaRealtimeOutputPlanningDraft& output,
    MediaAvSyncPlan synchronization,
    MediaAvSyncPlan outputSynchronization,
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
    if (!facts.value().timing.acknowledgementTimeout ||
        !facts.value().timing.terminalDrainWindow ||
        !synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V generation transition timing facts are incomplete"));
    }
    std::optional<MediaAudioCorrectionReachabilityResult> correction;
    if (audio.branchMode == MediaBranchMode::TranscodeFrame) {
        auto selected = MediaAudioCorrectionReachabilityPlanner::plan(
            synchronization, facts.value().timing);
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
    if (!audio.resolvedOutput ||
        (audio.branchMode == MediaBranchMode::TranscodeFrame && !audio.selectedResampler)) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::invalidArgument("audio output requires resolved format and selected processing"));
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
    auto plannedOutput = MediaRealtimeAvOutputRuntimePlanner::plan(
        {groupKey, *audio.resolvedOutput, outer.queues, edgePolicies.value(), facts.value().timing,
         correction ? std::optional<std::int64_t>(correction->maximumOutputBlockSamples) : std::nullopt,
         outer.outputLayout, outer.outputTransport, outputFrameRate, *outer.deployment, preparedEmission},
        std::move(outputSynchronization), output);
    if (!plannedOutput) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(plannedOutput.error());
    }
    auto outputPlan = std::move(plannedOutput).value();
    if (outputPlan.outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        outer.videoParameters.globalHeader = true;
    }
    auto transition = MediaAvGenerationTransitionPlanner::plan(
        outputPlan.protocolOutput,
        *synchronization.sourceClockMode,
        audio.branchMode,
        outer.videoPlan.filterActive,
        *facts.value().timing.acknowledgementTimeout,
        *facts.value().timing.terminalDrainWindow);
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
          MediaRealtimeAvSourceRuntimePlan{
            std::move(audio),
            std::move(outer.isolatedAudioInput),
            groupKey,
            std::move(synchronization),
            std::move(facts.value().assembly),
            outer.queues,
            std::move(edgePolicies).value(),
            outer.threadingPolicy,
            outputPlan.activationOutputLead,
            outer.videoPlan.filterActive,
            std::move(transition),
            facts.value().timing.inputAudioSampleRate,
            correction
                ? std::optional<MediaAudioCorrectionReachabilityPlan>(
                      correction->correction)
                : std::nullopt}, *outer.avSyncComponentBounds, facts.value().timing, outputPlan.outputAdapter, std::move(outputPlan.protocolOutput),
            std::move(outputPlan.datagramTransport), std::move(outputPlan.encoderFifoRetention)});
}

} // namespace media::ffmpeg::graph
