#include "internal/graph/planner/realtime/MediaCompositionTopologyPlanningKernel.h"
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaCompositionTopologyPlanningProducts> MediaCompositionTopologyPlanningKernel::plan(
    std::span<const MediaCompositionTopologySourceFacts> sources,
    const MediaAvContinuousAggregateTopology& aggregate,
    const MediaRealtimeAvOutputRuntimePlan& outputRuntime)
try {
    using Result = ::media::Result<MediaCompositionTopologyPlanningProducts>;
    if (sources.empty() || sources.size() != aggregate.sources.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition topology source declarations differ from the aggregate"));
    std::vector<MediaRealtimeEdgePolicySet> sourcePolicies;
    for (const auto& source : sources) sourcePolicies.push_back(source.policies);
    auto aggregateConnections = MediaCompositionAggregateConnectionsPlanner::plan(
        aggregate.sources, aggregate.audioSource, aggregate.audioPort, sourcePolicies, outputRuntime.edgePolicies);
    if (!aggregateConnections) return Result::failure(aggregateConnections.error());
    std::vector<MediaCompositionSourceTopology> sourceTopologies;
    for (std::size_t i=0; i<sources.size(); ++i) {
        const auto& source = sources[i];
        const auto& sync = source.synchronization;
        if (!sync.members || !sync.startup.videoReadiness || !sync.sourceClockMode)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source topology requires declared RTP startup facts"));
        const MediaCompositionSourceTopologyFacts facts{
            *sync.members, *sync.startup.videoReadiness, *sync.sourceClockMode,
            source.policies, source.branches, source.inputs,
            aggregateConnections.value().sources[i]};
        auto projected=MediaCompositionSourceTopologyPlanner::plan(facts);
        if (!projected) return Result::failure(projected.error());
        sourceTopologies.push_back(std::move(projected).value());
    }
    const auto videoShape=MediaVideoBranchConnectionsPlanner::outputEncoder();
    const auto audioShape=MediaAudioBranchConnectionsPlanner::outputEncoder(true);
    const auto& policies=outputRuntime.edgePolicies;
    auto videoConnections=MediaVideoBranchConnectionsPlanner::plan(videoShape,{policies.metadata,
        policies.videoPacket,policies.videoFrame,policies.videoFrame,policies.preparedVideoFrame});
    if(!videoConnections)return Result::failure(videoConnections.error());
    auto audioConnections=MediaAudioBranchConnectionsPlanner::plan(audioShape,policies);
    if(!audioConnections)return Result::failure(audioConnections.error());
    auto schedulerDeclaration=MediaOutputSegmentPlanner::scheduler(policies,outputRuntime.canonicalAudioInputPolicy);
    if(!schedulerDeclaration)return Result::failure(schedulerDeclaration.error());
    auto executionDeclaration=MediaOutputSegmentPlanner::execution(policies);
    if(!executionDeclaration)return Result::failure(executionDeclaration.error());
    auto protocolDeclaration=[&]()->::media::Result<MediaOutputSegmentDeclaration>{
        if(outputRuntime.outputAdapter==MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp)
            return MediaOutputSegmentPlanner::rtp(true,MediaPayloadKind::CodecContext,policies);
        const auto* mpegts=std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(&outputRuntime.protocolOutput);
        if(outputRuntime.outputAdapter!=MediaAvSyncOutputAdapterKind::ProjectMpegTs || !mpegts)
            return ::media::Result<MediaOutputSegmentDeclaration>::failure(::media::ErrorInfo::invalidArgument(
                "Composition output has no declared protocol topology"));
        return MediaOutputSegmentPlanner::mpegTs(true,std::holds_alternative<MediaMpegTsRtpOutputPlan>(mpegts->transport),
            MediaPayloadKind::CodecContext,policies);
    }();
    if(!protocolDeclaration)return Result::failure(protocolDeclaration.error());
    auto graphTopology=MediaCompositionGraphTopologyPlanner::plan({sourceTopologies,aggregateConnections.value(),
        videoShape,videoConnections.value(),audioShape,audioConnections.value(),schedulerDeclaration.value(),
        protocolDeclaration.value(),executionDeclaration.value()});
    if(!graphTopology)return Result::failure(graphTopology.error());
    return Result::success({std::move(aggregateConnections).value(),
        std::move(sourceTopologies), std::move(graphTopology).value()});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionTopologyPlanningProducts>::failure(
        ::media::ErrorInfo::allocationFailed("Composition topology planning products"));
}
}
