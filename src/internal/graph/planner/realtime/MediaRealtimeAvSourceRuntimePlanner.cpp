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

::media::Result<MediaRealtimeAvSourcePlanningProducts>
MediaRealtimeAvSourceRuntimePlanner::planFacts(
    const MediaRealtimeAvSourceRuntimeRequest& request,
    MediaAudioPipelinePlan audio,
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput,
    MediaAvSyncPlan synchronization)
{
    using Result = ::media::Result<MediaRealtimeAvSourcePlanningProducts>;
    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if (!request.groupKey.valid() || request.videoStreamIndex < 0 ||
        request.activationOutputLead <= MediaRunningTime::fromNanoseconds(0) ||
        request.queues.packet == 0 || request.queues.frame == 0 ||
        request.queues.metadata == 0 ||
        audio.enabled != hasAudio ||
        (hasAudio && (audio.sourceStreamIndex < 0 || audio.branchMode != MediaBranchMode::TranscodeFrame ||
                      !audio.resolvedOutput || synchronization.startup.trimAudioToCommonStart != true ||
                      synchronization.audioServo.outputSampleRate != audio.resolvedOutput->sampleRate())) ||
        (!hasAudio && (audio.branchMode != MediaBranchMode::Drop || audio.resolvedOutput || isolatedAudioInput)) ||
        synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution ||
        (!hasAudio && synchronization.members != MediaTranscodeStreamSet::VideoOnly)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source runtime requires explicit source identity, queues, activation and frame audio contracts"));
    }
    if (auto status = MediaAvSyncPlanValidator::validatePolicy(synchronization); !status)
        return Result::failure(status.error());
    if (auto status = MediaRealtimeAvSyncRuntimePlanValidator::validateThreading(request.threadingPolicy);
        !status) return Result::failure(status.error());
    const MediaRealtimeAvSourceClockRequest clockRequest{
        request.inputType, request.inputLayout, request.input, request.videoStreamIndex,
        request.videoCodecName, audio, isolatedAudioInput ? &*isolatedAudioInput : nullptr,
        request.sourceCapacity};
    auto sourceClock = MediaRealtimeAvSourceClockPlanner::plan(clockRequest, synchronization);
    if (!sourceClock) return Result::failure(sourceClock.error());
    if (auto status = MediaRealtimeAvSyncRuntimeInputValidator::validate(
            clockRequest, synchronization, sourceClock.value().timing, sourceClock.value().assembly);
        !status) return Result::failure(status.error());
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
    if (hasAudio) {
        auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(request.queues, audio);
        if (!bounds) return Result::failure(bounds.error());
        auto correction = MediaAudioCorrectionReachabilityPlanner::plan(synchronization,
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
        request.queues, synchronization.startup, *synchronization.members);
    if (!edges) return Result::failure(edges.error());
    if(const auto* capacity=request.sourceCapacity){
        const auto& planned=capacity->queues;
        if(std::tie(request.queues.metadata,request.queues.packet,request.queues.frame,request.queues.mux)!=
                std::tie(planned.metadata,planned.packet,planned.frame,planned.mux) ||
            capacity->route.metadata!=edges.value().metadata || capacity->route.atomicMetadata!=edges.value().atomicMetadata)
            return Result::failure(::media::ErrorInfo::invalidArgument("Source runtime route or queues differ from capacity facts"));
    }

    auto transition = MediaAvGenerationTransitionPlanner::planSourceContribution(
        *synchronization.sourceClockMode, audio.branchMode, request.videoFilterActive,
        *synchronization.startup.videoReadiness, *synchronization.recovery.reacquisitionTimeoutNs, request.selectedAudio);
    if (!transition) return Result::failure(transition.error());
    return Result::success({
        std::move(audio), std::move(isolatedAudioInput), request.groupKey,
        std::move(synchronization), std::move(sourceClock.value().assembly),
        request.queues, std::move(edges).value(), request.threadingPolicy,
        request.activationOutputLead, request.videoFilterActive, std::move(transition).value(),
        sourceClock.value().timing.inputAudioSampleRate, std::move(audioCorrection)});
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
