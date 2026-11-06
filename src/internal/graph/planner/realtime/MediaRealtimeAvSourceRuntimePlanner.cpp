#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeInputValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"

#include <utility>
#include <tuple>

namespace media::ffmpeg::graph {

namespace {
::media::Status validateSource(const MediaAvSyncGroupKey& groupKey, int videoStreamIndex,
    MediaRunningTime activationOutputLead, const MediaGraphQueueParameters& queues,
    const MediaThreadingPolicy& threading, const MediaAudioPipelinePlan& audio,
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput, const MediaAvSyncPlan& synchronization)
{
    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if (!groupKey.valid() || videoStreamIndex < 0 ||
        activationOutputLead <= MediaRunningTime::fromNanoseconds(0) ||
        queues.packet == 0 || queues.frame == 0 ||
        queues.metadata == 0 ||
        audio.enabled != hasAudio ||
        (hasAudio && (audio.sourceStreamIndex < 0 || audio.branchMode != MediaBranchMode::TranscodeFrame ||
                      !audio.resolvedOutput || synchronization.startup.trimAudioToCommonStart != true ||
                      synchronization.audioServo.outputSampleRate != audio.resolvedOutput->sampleRate())) ||
        (!hasAudio && (audio.branchMode != MediaBranchMode::Drop || audio.resolvedOutput || isolatedAudioInput)) ||
        synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution ||
        (!hasAudio && synchronization.members != MediaTranscodeStreamSet::VideoOnly)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source runtime requires explicit source identity, queues, activation and frame audio contracts"));
    }
    if (auto status = MediaAvSyncPlanValidator::validatePolicy(synchronization); !status)
        return ::media::Status::failure(status.error());
    if (auto status = MediaRealtimeAvSyncRuntimePlanValidator::validateThreading(threading);
        !status) return ::media::Status::failure(status.error());
    return ::media::Status::success();
}
struct SourceCapacityPolicies final {
    MediaRealtimeEdgePolicySet edges;
    std::optional<MediaAudioCorrectionReachabilityPlan> correction;
};
::media::Result<SourceCapacityPolicies> planCapacityPolicies(
    const MediaGraphQueueParameters& queues, const MediaAudioPipelinePlan& audio,
    const MediaAvSourceCapacityFacts* sourceCapacity, MediaAvSyncPlan& synchronization)
{
    using Result = ::media::Result<SourceCapacityPolicies>;
    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
    if (hasAudio) {
        auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(queues, audio);
        if (!bounds) return Result::failure(bounds.error());
        auto correction = MediaAudioCorrectionReachabilityPlanner::plan(synchronization.audioServo,
            MediaAudioSourceCorrectionFacts{audio.resolvedOutput->sampleRate(), bounds.value()});
        if (!correction) return Result::failure(correction.error());
        synchronization.audioServo.commandLeadNs = correction.value().commandLead;
        synchronization.audioServo.compensationWindowNs = correction.value().compensationWindow;
        synchronization.audioServo.frequencyFilterTimeConstantNs = correction.value().frequencyFilterTimeConstant;
        audioCorrection = correction.value().correction;
    }
    if (auto status = MediaAvSyncPlanValidator::validateDomain(
            synchronization, MediaAvSyncDomainRole::SourceContribution); !status)
        return Result::failure(status.error());
    auto edges = MediaRealtimeEdgePolicyPlanner::planSynchronizedSource(
        queues, synchronization.startup, *synchronization.members);
    if (!edges) return Result::failure(edges.error());
    if(const auto* capacity=sourceCapacity){
        const auto& planned=capacity->queues;
        if(std::tie(queues.metadata,queues.packet,queues.frame,queues.mux)!=
                std::tie(planned.metadata,planned.packet,planned.frame,planned.mux) ||
            capacity->route.metadata!=edges.value().metadata || capacity->route.atomicMetadata!=edges.value().atomicMetadata)
            return Result::failure(::media::ErrorInfo::invalidArgument("Source runtime route or queues differ from capacity facts"));
    }

    return Result::success({std::move(edges).value(), std::move(audioCorrection)});
}
// Only six capacity fields may vary. Identity is retained by the owning plan.
auto fixedStartupFields(const MediaAvSyncStartupPolicy& policy)
{
    return std::tie(policy.videoReadiness, policy.requireVideoKeyFrame, policy.trimAudioToCommonStart,
        policy.maximumWaitNs, policy.prerollNs, policy.keyFrameWaitNs, policy.maximumAudioTrimNs,
        policy.maximumInitialSkewNs, policy.maximumGapNs, policy.outputLeadNs, policy.allowDegradedClock);
}
} // namespace

::media::Result<MediaRealtimeAvSourcePlanningProducts>
MediaRealtimeAvSourceRuntimePlanner::planFacts(
    const MediaRealtimeAvSourceRuntimeRequest& request,
    MediaAudioPipelinePlan audio,
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput,
    MediaAvSyncPlan synchronization)
{
    using Result = ::media::Result<MediaRealtimeAvSourcePlanningProducts>;
    if (auto status = validateSource(request.groupKey, request.videoStreamIndex, request.activationOutputLead,
            request.queues, request.threadingPolicy, audio,
            isolatedAudioInput ? &*isolatedAudioInput : nullptr, synchronization); !status)
        return Result::failure(status.error());
    const MediaRealtimeAvSourceClockRequest clockRequest{
        request.inputType, request.inputLayout, request.input, request.videoStreamIndex,
        request.videoCodecName, audio, isolatedAudioInput ? &*isolatedAudioInput : nullptr,
        request.sourceCapacity};
    auto sourceClock = MediaRealtimeAvSourceClockPlanner::plan(clockRequest, synchronization);
    if (!sourceClock) return Result::failure(sourceClock.error());
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            clockRequest, synchronization, sourceClock.value().timing, sourceClock.value().assembly);
        !status) return Result::failure(status.error());
    auto capacityPolicies = planCapacityPolicies(request.queues, audio, request.sourceCapacity, synchronization);
    if (!capacityPolicies) return Result::failure(capacityPolicies.error());

    auto transition = MediaAvGenerationTransitionPlanner::planSourceContribution(
        *synchronization.sourceClockMode, audio.branchMode, request.videoFilterActive,
        *synchronization.startup.videoReadiness, *synchronization.recovery.reacquisitionTimeoutNs, request.selectedAudio);
    if (!transition) return Result::failure(transition.error());
    return Result::success({
        std::move(audio), std::move(isolatedAudioInput), request.groupKey,
        std::move(synchronization), std::move(sourceClock.value().assembly),
        request.queues, std::move(capacityPolicies.value().edges), request.threadingPolicy,
        request.activationOutputLead, request.videoFilterActive, std::move(transition).value(),
        sourceClock.value().timing.inputAudioSampleRate, std::move(capacityPolicies.value().correction)});
}

::media::Status MediaRealtimeAvSourceRuntimePlanner::rebindCapacity(
    const MediaRealtimeAvSourceClockRequest& clockRequest,
    const MediaRealtimeAvSourceTimingFacts& timing, const MediaAvSyncStartupPolicy& startup,
    MediaRealtimeAvSourcePlanningProducts& products)
{
    const auto* capacity = clockRequest.sourceCapacity;
    auto& synchronization = products.synchronization;
    auto& current = synchronization.startup;
    const auto* isolated = products.isolatedAudioInput ? &*products.isolatedAudioInput : nullptr;
    if (!capacity || &clockRequest.audio != &products.audioPipeline || clockRequest.isolatedAudioInput != isolated ||
        fixedStartupFields(startup) != fixedStartupFields(current) ||
        (startup.videoIdentity && startup.videoIdentity != current.videoIdentity) ||
        (startup.audioIdentity && startup.audioIdentity != current.audioIdentity) ||
        timing.inputAudioSampleRate != products.inputAudioSampleRate)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source capacity refresh changed fixed source facts"));
    current.videoCapacity = startup.videoCapacity;
    current.audioCapacity = startup.audioCapacity;
    current.videoByteCapacity = startup.videoByteCapacity;
    current.audioByteCapacity = startup.audioByteCapacity;
    current.maximumVideoUnitBytes = startup.maximumVideoUnitBytes;
    current.maximumAudioUnitBytes = startup.maximumAudioUnitBytes;
    if (auto status = validateSource(products.groupKey, clockRequest.videoStreamIndex, products.activationOutputLead,
            capacity->queues, products.threadingPolicy, products.audioPipeline, isolated, synchronization); !status) return status;
    if (auto status = MediaRealtimeAvSourceClockPlanner::rebindAssembly(
            clockRequest, synchronization, timing, products.assembly); !status) return status;
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            clockRequest, synchronization, timing, products.assembly); !status) return status;
    auto policies = planCapacityPolicies(capacity->queues, products.audioPipeline, capacity, synchronization);
    if (!policies) return ::media::Status::failure(policies.error());
    products.queues = capacity->queues;
    products.edgePolicies = std::move(policies.value().edges);
    products.audioCorrection = std::move(policies.value().correction);
    return ::media::Status::success();
}

::media::Result<MediaRealtimeAvSourceRuntimePlan> MediaRealtimeAvSourceRuntimePlanner::bind(
    MediaRealtimeAvSourcePlanningProducts products, MediaOutputReservationStoragePlan reservationStorage)
{
    using Result = ::media::Result<MediaRealtimeAvSourceRuntimePlan>;
    const auto& synchronization = products.synchronization;
    // This checks the reservation's internal domain contract only. The source
    // planner binds its sealed owner; the final graph binder independently
    // compares this resource with the actual RTP session's reference resource.
    if (auto valid = MediaOutputReservationStoragePlanner::validate(reservationStorage, reservationStorage.allocation()); !valid)
        return Result::failure(valid.error());
    const bool prepared = std::holds_alternative<MediaPreparedLineageAllocation>(reservationStorage.allocation());
    if ((synchronization.startup.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan) != prepared ||
        (synchronization.startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan &&
         synchronization.startup.videoReadiness != MediaVideoStartupReadiness::IntervalCoverage))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source reservation storage differs from planned readiness"));
    return Result::success({std::move(products), std::move(reservationStorage)});
}

} // namespace media::ffmpeg::graph
