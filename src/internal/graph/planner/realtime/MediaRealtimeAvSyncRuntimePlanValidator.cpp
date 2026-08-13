#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeOutputValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include <cstdint>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeAvSyncRuntimePlanValidator::validate(
    const MediaRealtimeRtpTranscodePlan& outer,
    const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    const auto invalid = [](const char* field) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Invalid synchronized runtime product: ") + field));
    };
    if (runtime.audioPipeline.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime product rejects audio packet copy"));
    }
    if (outer.videoPlan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "Synchronized runtime product rejects video packet copy"));
    }
    if (runtime.groupKey.value() != "realtime.av" ||
        !MediaAvSyncPlanValidator::validate(runtime.synchronization)) {
        return invalid("group or synchronization plan");
    }
    auto selectedBounds = MediaRealtimeAvSyncComponentBoundsPlanner::plan(
        runtime.queues, runtime.audioPipeline);
    if (!selectedBounds ||
        selectedBounds.value() != runtime.componentBounds) {
        return invalid("selected component bounds");
    }
    MediaRealtimeOutputPlanningDraft selectedOutput;
    if (runtime.planningFacts.outputVideoRtpPacketization &&
        runtime.planningFacts.outputAudioRtpPacketization) {
        selectedOutput.videoOutput.scheduledPacketization =
            runtime.planningFacts.outputVideoRtpPacketization;
        selectedOutput.audioOutput.scheduledPacketization =
            runtime.planningFacts.outputAudioRtpPacketization;
    }
    auto selectedFacts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, runtime.audioPipeline, runtime.componentBounds,
        runtime.isolatedAudioInput ? &*runtime.isolatedAudioInput : nullptr,
        selectedOutput,
        runtime.synchronization);
    if (!selectedFacts || selectedFacts.value() != runtime.planningFacts) {
        return invalid("selected planning facts");
    }
    const auto& assembly = runtime.assembly;
    if (assembly.generationPolicy !=
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange ||
        assembly.initialGeneration != MediaFirstLockedSourceGeneration ||
        assembly.evidencePolicy !=
            MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire ||
        assembly.video.sourceIdentity.empty() ||
        assembly.audio.sourceIdentity.empty() ||
        !runtime.synchronization.startup.videoIdentity ||
        !runtime.synchronization.startup.audioIdentity ||
        assembly.video.sourceIdentity !=
            *runtime.synchronization.startup.videoIdentity ||
        assembly.audio.sourceIdentity !=
            *runtime.synchronization.startup.audioIdentity ||
        runtime.planningFacts.inputVideoIdentity !=
            runtime.synchronization.startup.videoIdentity ||
        runtime.planningFacts.inputAudioIdentity !=
            runtime.synchronization.startup.audioIdentity ||
        assembly.video.decodeOrder !=
            MediaDecodeOrderMode::ReorderedRequiresDecodeTime ||
        assembly.audio.decodeOrder !=
            MediaDecodeOrderMode::PresentationOrderNoReorder ||
        !runtime.synchronization.startup.videoCapacity ||
        !runtime.synchronization.startup.audioCapacity ||
        assembly.video.acquiringCapacity == 0 ||
        assembly.audio.acquiringCapacity == 0 ||
        assembly.video.acquiringCapacity !=
            *runtime.synchronization.startup.videoCapacity ||
        assembly.audio.acquiringCapacity !=
            *runtime.synchronization.startup.audioCapacity ||
        !runtime.synchronization.startup.maximumWaitNs ||
        assembly.video.acquiringTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.audio.acquiringTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.video.acquiringTimeout !=
            *runtime.synchronization.startup.maximumWaitNs ||
        assembly.audio.acquiringTimeout !=
            *runtime.synchronization.startup.maximumWaitNs ||
        !runtime.synchronization.audioServo.minimumUpdateIntervalNs ||
        assembly.startupClockInterval <=
            MediaRunningTime::fromNanoseconds(0) ||
        assembly.startupClockInterval !=
            *runtime.synchronization.audioServo.minimumUpdateIntervalNs) {
        return invalid("production assembly common contract");
    }
    auto expectedCorrection = MediaAudioCorrectionReachabilityPlanner::plan(
        runtime.synchronization, runtime.planningFacts);
    if (!expectedCorrection ||
        runtime.audioCorrection != expectedCorrection.value().correction ||
        !runtime.synchronization.audioServo.commandLeadNs ||
        !runtime.synchronization.audioServo.compensationWindowNs ||
        !runtime.synchronization.audioServo.frequencyFilterTimeConstantNs ||
        *runtime.synchronization.audioServo.commandLeadNs !=
            expectedCorrection.value().commandLead ||
        *runtime.synchronization.audioServo.compensationWindowNs !=
            expectedCorrection.value().compensationWindow ||
        *runtime.synchronization.audioServo.frequencyFilterTimeConstantNs !=
            expectedCorrection.value().frequencyFilterTimeConstant) {
        return invalid("audio correction derivation");
    }
    if (runtime.edgePolicies !=
        MediaRealtimeEdgePolicyPlanner::plan(runtime.queues)) {
        return invalid("edge-policy product");
    }
    if (runtime.threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        runtime.threadingPolicy.priority != MediaThreadPriority::High ||
        runtime.threadingPolicy.maxWorkerThreads != 0 ||
        runtime.threadingPolicy.pinWorkers ||
        !runtime.threadingPolicy.collectWorkerMetrics) {
        return invalid("threading product");
    }
    if (runtime.transition.acknowledgementTimeout <=
            MediaRunningTime::fromNanoseconds(0) ||
        runtime.transition.terminalDrainWindow <=
            MediaRunningTime::fromNanoseconds(0) ||
        !runtime.planningFacts.acknowledgementTimeout ||
        !runtime.planningFacts.terminalDrainWindow ||
        runtime.transition.acknowledgementTimeout !=
            *runtime.planningFacts.acknowledgementTimeout ||
        runtime.transition.terminalDrainWindow !=
            *runtime.planningFacts.terminalDrainWindow) {
        return invalid("transition timeout");
    }
    if (runtime.outputAdapter !=
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp &&
        runtime.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        return invalid("output adapter");
    }
    const auto expected = MediaAvGenerationTransitionPlanner::plan(
        runtime.outputAdapter,
        *runtime.synchronization.sourceClockMode,
        runtime.videoFilterActive,
        runtime.transition.acknowledgementTimeout,
        runtime.transition.terminalDrainWindow);
    if (runtime.transition.participants.size() !=
        expected.participants.size()) {
        return invalid("transition participant count");
    }
    for (std::size_t index = 0; index < expected.participants.size(); ++index) {
        if (runtime.transition.participants[index].participant !=
                expected.participants[index].participant ||
            runtime.transition.participants[index].requiredChildren !=
                expected.participants[index].requiredChildren) {
            return invalid("transition participant children");
        }
    }
    const auto& correction = runtime.audioCorrection;
    const bool reachabilitySumRepresentable =
        correction.worstCaseInFlightSamples >= 0 &&
        correction.mailboxDeliveryMarginSamples > 0 &&
        correction.maximumResamplerOutputBlockSamples > 0 &&
        correction.worstCaseInFlightSamples <=
            std::numeric_limits<std::int64_t>::max() -
                correction.mailboxDeliveryMarginSamples &&
        correction.worstCaseInFlightSamples +
                correction.mailboxDeliveryMarginSamples <=
            std::numeric_limits<std::int64_t>::max() -
                correction.maximumResamplerOutputBlockSamples;
    if (correction.outputSampleRate <= 0 ||
        correction.epochOutputSampleIndex != 0 ||
        correction.worstCaseInFlightSamples < 0 ||
        correction.protocolBatchSamples <= 0 ||
        correction.mailboxDeliveryMarginSamples <= 0 ||
        correction.maximumResamplerOutputBlockSamples <= 0 ||
        correction.mailboxCapacity != runtime.queues.metadata ||
        !reachabilitySumRepresentable ||
        correction.commandLeadSamples <=
            correction.worstCaseInFlightSamples +
                correction.mailboxDeliveryMarginSamples +
                correction.maximumResamplerOutputBlockSamples ||
        !runtime.synchronization.audioServo.outputSampleRate ||
        correction.outputSampleRate !=
            *runtime.synchronization.audioServo.outputSampleRate) {
        return invalid("audio correction reachability");
    }
    if (auto inputStatus =
            MediaRealtimeAvSyncRuntimeInputValidator::validate(
                outer, runtime);
        !inputStatus) {
        return inputStatus;
    }
    if (auto outputStatus =
            MediaRealtimeAvSyncRuntimeOutputValidator::validate(
                outer, runtime);
        !outputStatus) {
        return outputStatus;
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
