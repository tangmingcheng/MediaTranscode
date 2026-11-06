#include "internal/graph/planner/realtime/MediaCompositionTopologyPlanningKernel.h"
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
template<bool Updating> ::media::Status compileTopology(
    std::span<const MediaCompositionTopologySourceFacts> sources,
    const MediaAvContinuousAggregateTopology& aggregate,
    MediaCompositionTopologyPlanningProducts& products)
{
    using Result = ::media::Status;
    if (sources.empty() || sources.size() != aggregate.sources.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition topology source declarations differ from the aggregate"));
    if constexpr (Updating) {
        if (products.sourcePolicies.size() != sources.size() || products.sources.size() != sources.size())
            return Result::failure(::media::ErrorInfo::invalidArgument("Composition topology source catalog changed"));
        for (std::size_t i=0; i<sources.size(); ++i) products.sourcePolicies[i] = sources[i].policies;
        if (auto status=MediaCompositionAggregateConnectionsPlanner::rebind(aggregate.sources,
                aggregate.audioSource, aggregate.audioPort, products.sourcePolicies,
                products.output.policies, products.aggregate); !status) return status;
    } else {
        for (const auto& source : sources) products.sourcePolicies.push_back(source.policies);
        auto connections=MediaCompositionAggregateConnectionsPlanner::plan(aggregate.sources,
            aggregate.audioSource, aggregate.audioPort, products.sourcePolicies, products.output.policies, products.graph.resource());
        if (!connections) return Result::failure(connections.error());
        products.aggregate=std::move(connections).value();
    }
    for (std::size_t i=0; i<sources.size(); ++i) {
        const auto& source=sources[i];
        const auto& sync=source.synchronization;
        if (!sync.members || !sync.startup.videoReadiness || !sync.sourceClockMode)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source topology requires declared RTP startup facts"));
        const MediaCompositionSourceTopologyFacts facts{
            *sync.members, *sync.startup.videoReadiness, *sync.sourceClockMode,
            source.policies, source.branches, source.inputs, products.aggregate.sources[i]};
        if constexpr (Updating) {
            if (auto status=MediaCompositionSourceTopologyPlanner::rebind(facts, products.sources[i]); !status) return status;
        } else {
            auto projected=MediaCompositionSourceTopologyPlanner::plan(facts, products.graph.resource());
            if (!projected) return Result::failure(projected.error());
            products.sources.push_back(std::move(projected).value());
        }
    }
    const auto& output=products.output;
    const MediaCompositionGraphTopologyFacts facts{products.sources, products.aggregate,
        output.videoShape, output.videoConnections, output.audioShape, output.audioConnections,
        output.scheduler, output.protocol, output.execution};
    if constexpr (Updating) {
        return MediaCompositionGraphTopologyPlanner::rebind(facts, products.graph);
    } else {
        auto graph=MediaCompositionGraphTopologyPlanner::plan(facts, products.graph.resource());
        if (!graph) return Result::failure(graph.error());
        products.graph=std::move(graph).value();
        return Result::success();
    }
}
} // namespace
::media::Result<MediaCompositionTopologyPlanningProducts> MediaCompositionTopologyPlanningKernel::plan(
    std::span<const MediaCompositionTopologySourceFacts> sources,
    const MediaAvContinuousAggregateTopology& aggregate,
    const MediaRealtimeAvOutputRuntimePlan& outputRuntime,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    using Result = ::media::Result<MediaCompositionTopologyPlanningProducts>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("Topology planning requires a preparation resource"));
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
    MediaCompositionTopologyPlanningProducts products(std::move(resource));
    products.output={policies,videoShape,std::move(videoConnections).value(),
        audioShape,std::move(audioConnections).value(),std::move(schedulerDeclaration).value(),
        std::move(protocolDeclaration).value(),std::move(executionDeclaration).value()};
    if (auto status=compileTopology<false>(sources,aggregate,products); !status)
        return Result::failure(status.error());
    return Result::success(std::move(products));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionTopologyPlanningProducts>::failure(
        ::media::ErrorInfo::allocationFailed("Composition topology planning products"));
}
::media::Status MediaCompositionTopologyPlanningKernel::rebind(
    std::span<const MediaCompositionTopologySourceFacts> sources,
    const MediaAvContinuousAggregateTopology& aggregate,
    MediaCompositionTopologyPlanningProducts& products)
{
    return compileTopology<true>(sources,aggregate,products);
}

}
