#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeAvSourceRuntimePlan>
MediaRealtimeAvSourceRuntimePlanner::plan(
    const MediaRealtimeAvSourceRuntimeRequest& request,
    MediaAudioPipelinePlan audio,
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput,
    MediaAvSyncPlan synchronization)
{
    using Result = ::media::Result<MediaRealtimeAvSourceRuntimePlan>;
    if (!request.groupKey.valid() || request.videoStreamIndex < 0 ||
        request.activationOutputLead <= MediaRunningTime::fromNanoseconds(0) ||
        request.queues.packet == 0 || request.queues.frame == 0 ||
        request.queues.metadata == 0 ||
        !audio.enabled || audio.sourceStreamIndex < 0 || audio.branchMode != MediaBranchMode::TranscodeFrame ||
        !audio.resolvedOutput ||
        synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution ||
        synchronization.startup.trimAudioToCommonStart != true ||
        synchronization.audioServo.outputSampleRate != audio.resolvedOutput->sampleRate()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source runtime requires explicit source identity, queues, activation and frame audio contracts"));
    }
    if (auto status = MediaAvSyncPlanValidator::validatePolicy(synchronization); !status)
        return Result::failure(status.error());
    if (auto status = MediaRealtimeAvSyncRuntimePlanValidator::validateThreading(request.threadingPolicy);
        !status) return Result::failure(status.error());
    const MediaRealtimeAvSourceClockRequest clockRequest{
        request.inputType, request.inputLayout, request.input, request.videoStreamIndex,
        request.videoCodecName, audio, isolatedAudioInput ? &*isolatedAudioInput : nullptr};
    auto sourceClock = MediaRealtimeAvSourceClockPlanner::plan(clockRequest, synchronization);
    if (!sourceClock) return Result::failure(sourceClock.error());
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            clockRequest, synchronization, sourceClock.value().timing, sourceClock.value().assembly);
        !status) return Result::failure(status.error());
    auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(request.queues, audio);
    if (!bounds) return Result::failure(bounds.error());
    auto correction = MediaAudioCorrectionReachabilityPlanner::plan(synchronization,
        MediaAudioSourceCorrectionFacts{audio.resolvedOutput->sampleRate(), bounds.value()});
    if (!correction) return Result::failure(correction.error());
    synchronization.audioServo.commandLeadNs = correction.value().commandLead;
    synchronization.audioServo.compensationWindowNs = correction.value().compensationWindow;
    synchronization.audioServo.frequencyFilterTimeConstantNs = correction.value().frequencyFilterTimeConstant;
    if (auto status = MediaAvSyncPlanValidator::validateDomain(
            synchronization, MediaAvSyncDomainRole::SourceContribution); !status)
        return Result::failure(status.error());
    auto edges = MediaRealtimeEdgePolicyPlanner::planSynchronizedSource(
        request.queues, synchronization.startup);
    if (!edges) return Result::failure(edges.error());
    auto transition = MediaAvGenerationTransitionPlanner::planSourceContribution(
        *synchronization.sourceClockMode, audio.branchMode, request.videoFilterActive,
        *synchronization.recovery.reacquisitionTimeoutNs,
        *synchronization.audioServo.maximumMeasurementGapNs);
    if (!transition) return Result::failure(transition.error());
    return Result::success({
        std::move(audio), std::move(isolatedAudioInput), request.groupKey,
        std::move(synchronization), std::move(sourceClock.value().assembly),
        request.queues, std::move(edges).value(), request.threadingPolicy,
        request.activationOutputLead, request.videoFilterActive, std::move(transition).value(),
        sourceClock.value().timing.inputAudioSampleRate, correction.value().correction});
}

} // namespace media::ffmpeg::graph
