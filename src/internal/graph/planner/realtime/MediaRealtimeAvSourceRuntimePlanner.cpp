#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
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
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput, const MediaAvSyncPlanView& synchronization)
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
    const MediaAvSourceCapacityFacts* sourceCapacity, MediaAvSyncPlanView& synchronization)
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
// Capacity quantities and maximum gap may vary. Identity is retained by the owning plan.
template<class Policy> auto fixedStartupFields(const Policy& policy)
{
    return std::tie(policy.videoReadiness, policy.requireVideoKeyFrame, policy.trimAudioToCommonStart,
        policy.maximumWaitNs, policy.prerollNs, policy.keyFrameWaitNs, policy.maximumAudioTrimNs,
        policy.maximumInitialSkewNs, policy.outputLeadNs, policy.allowDegradedClock);
}
} // namespace

::media::Result<MediaRealtimeAvSourceFixedProducts>
MediaRealtimeAvSourceRuntimePlanner::prepareFixed(
    const MediaRealtimeAvSourceRuntimeFixedRequest& request, MediaAudioPipelinePlan audio,
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput, MediaAvSyncFixedPlan synchronization)
{
    using Result = ::media::Result<MediaRealtimeAvSourceFixedProducts>;
    const MediaRealtimeAvSourceClockFixedRequest clockRequest{
        request.inputType, request.inputLayout, request.input, request.videoStreamIndex,
        request.videoCodecName, audio, isolatedAudioInput ? &*isolatedAudioInput : nullptr};
    auto clock = MediaRealtimeAvSourceClockPlanner::prepareFixed(clockRequest, synchronization);
    if (!clock) return Result::failure(clock.error());
    if (!request.groupKey.valid() || !synchronization.sourceClockMode ||
        !synchronization.startup.videoReadiness || !synchronization.recovery.reacquisitionTimeoutNs)
        return Result::failure(::media::ErrorInfo::notInitialized("Fixed source runtime lacks identity or transition facts"));
    auto transition = MediaAvGenerationTransitionPlanner::planSourceContribution(
        *synchronization.sourceClockMode, audio.branchMode, request.videoFilterActive,
        *synchronization.startup.videoReadiness, *synchronization.recovery.reacquisitionTimeoutNs, request.selectedAudio);
    if (!transition) return Result::failure(transition.error());
    return Result::success({std::move(audio), std::move(isolatedAudioInput), request.groupKey,
        std::move(synchronization), std::move(clock).value(), request.threadingPolicy,
        request.activationOutputLead, request.videoFilterActive, std::move(transition).value()});
}

::media::Result<MediaRealtimeAvSourceCapacityState>
MediaRealtimeAvSourceRuntimePlanner::planCapacity(
    const MediaRealtimeAvSourceFixedProducts& fixed, const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncStartupCapacity& startup, const MediaGraphQueueParameters& queues)
{
    using Result = ::media::Result<MediaRealtimeAvSourceCapacityState>;
    const auto* isolated = fixed.isolatedAudioInput ? &*fixed.isolatedAudioInput : nullptr;
    if (&request.audio != &fixed.audioPipeline || request.isolatedAudioInput != isolated)
        return Result::failure(::media::ErrorInfo::invalidArgument("Source capacity requires its fixed audio owner"));
    MediaAvSyncPlanView synchronization(fixed.synchronization, startup);
    if (auto status = validateSource(fixed.groupKey, request.videoStreamIndex, fixed.activationOutputLead,
            queues, fixed.threadingPolicy, fixed.audioPipeline, isolated, synchronization); !status)
        return Result::failure(status.error());
    auto assembly = MediaRealtimeAvSourceClockPlanner::planCapacityView(fixed.clock, request, synchronization);
    if (!assembly) return Result::failure(assembly.error());
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            request, synchronization, fixed.clock.timing, assembly.value()); !status)
        return Result::failure(status.error());
    auto policies = planCapacityPolicies(queues, fixed.audioPipeline, request.sourceCapacity, synchronization);
    if (!policies) return Result::failure(policies.error());
    return Result::success(MediaRealtimeAvSourceCapacityState{fixed, startup, synchronization.audioServo,
        assembly.value(), queues, policies.value().edges, policies.value().correction});
}

::media::Result<MediaRealtimeAvSourcePlanningProducts>
MediaRealtimeAvSourceRuntimePlanner::bindFacts(
    MediaRealtimeAvSourceFixedProducts&& fixed, MediaRealtimeAvSourceCapacityState&& capacity)
{
    using Result = ::media::Result<MediaRealtimeAvSourcePlanningProducts>;
    const auto& candidate = capacity.assembly;
    if (!capacity.m_valid || !capacity.fixed.belongsTo(fixed) || fixed.clock.assembly.audio.has_value() != candidate.audio.has_value() ||
        &fixed.clock.assembly.inputClock != &candidate.inputClock ||
        &fixed.clock.assembly.video.sourceIdentity != &candidate.video.sourceIdentity ||
        &fixed.clock.assembly.video.timing != &candidate.video.timing ||
        (candidate.audio && (&fixed.clock.assembly.audio->sourceIdentity != &candidate.audio->sourceIdentity ||
            &fixed.clock.assembly.audio->duration != &candidate.audio->duration)))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source capacity binding requires its original fixed owner"));
    capacity.m_valid = false;
    fixed.synchronization.audioServo = capacity.audioServo;
    auto synchronization = MediaAvSyncPlanner::bind(std::move(fixed.synchronization), capacity.startup);
    if (!synchronization) return Result::failure(synchronization.error());
    auto clock = MediaRealtimeAvSourceClockPlanner::consumeCapacity(std::move(fixed.clock), candidate);
    if (!clock) return Result::failure(clock.error());
    return Result::success({std::move(fixed.audioPipeline), std::move(fixed.isolatedAudioInput), std::move(fixed.groupKey),
        std::move(synchronization).value(), std::move(clock.value().assembly), capacity.queues, capacity.edgePolicies,
        fixed.threadingPolicy, fixed.activationOutputLead, fixed.videoFilterActive, std::move(fixed.transition),
        clock.value().timing.inputAudioSampleRate, capacity.audioCorrection});
}

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
    const auto startup = static_cast<const MediaAvSyncStartupCapacity&>(synchronization.startup);
    MediaAvSyncFixedPlan fixedSynchronization{
        std::move(static_cast<MediaAvSyncFixedFields&>(synchronization)),
        std::move(static_cast<MediaAvSyncStartupFixedPolicy&>(synchronization.startup))};
    auto fixed = prepareFixed(request, std::move(audio), std::move(isolatedAudioInput), std::move(fixedSynchronization));
    if (!fixed) return Result::failure(fixed.error());
    const MediaRealtimeAvSourceClockRequest clockRequest{{
        request.inputType, request.inputLayout, request.input, request.videoStreamIndex,
        request.videoCodecName, fixed.value().audioPipeline,
        fixed.value().isolatedAudioInput ? &*fixed.value().isolatedAudioInput : nullptr}, request.sourceCapacity};
    auto capacity = planCapacity(fixed.value(), clockRequest, startup, request.queues);
    if (!capacity) return Result::failure(capacity.error());
    return bindFacts(std::move(fixed).value(), std::move(capacity).value());
}

::media::Status MediaRealtimeAvSourceRuntimePlanner::rebindCapacity(
    const MediaRealtimeAvSourceClockRequest& clockRequest,
    const MediaRealtimeAvSourceTimingFacts& timing, const MediaAvSyncStartupPolicyView& startup,
    MediaRealtimeAvSourceCapacityState& state)
{
    if (!state.m_valid)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source capacity candidate is invalid"));
    state.m_valid = false;
    const auto* capacity = clockRequest.sourceCapacity;
    const auto& fixed = state.fixed;
    const auto& current = fixed.startup;
    const auto* isolated = fixed.isolatedAudioInput ? &*fixed.isolatedAudioInput : nullptr;
    if (!capacity || &clockRequest.audio != &fixed.audioPipeline || clockRequest.isolatedAudioInput != isolated ||
        fixedStartupFields(startup) != fixedStartupFields(current) ||
        (startup.videoIdentity && startup.videoIdentity != current.videoIdentity) ||
        (startup.audioIdentity && startup.audioIdentity != current.audioIdentity) ||
        timing.inputAudioSampleRate != fixed.inputAudioSampleRate)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source capacity refresh changed fixed source facts"));
    state.startup = static_cast<const MediaAvSyncStartupCapacity&>(startup);
    MediaAvSyncPlanView synchronization(fixed.synchronization,
        MediaAvSyncStartupPolicyView(current, state.startup), state.audioServo);
    if (auto status = validateSource(fixed.groupKey, clockRequest.videoStreamIndex, fixed.activationOutputLead,
            capacity->queues, fixed.threadingPolicy, fixed.audioPipeline, isolated, synchronization); !status) return status;
    if (auto status = MediaRealtimeAvSourceClockPlanner::rebindAssembly(
            clockRequest, synchronization, timing, state.assembly); !status) return status;
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            clockRequest, synchronization, timing, state.assembly); !status) return status;
    auto policies = planCapacityPolicies(capacity->queues, fixed.audioPipeline, capacity, synchronization);
    if (!policies) return ::media::Status::failure(policies.error());
    state.audioServo = synchronization.audioServo;
    state.queues = capacity->queues;
    state.edgePolicies = std::move(policies.value().edges);
    state.audioCorrection = std::move(policies.value().correction);
    state.m_valid = true;
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
