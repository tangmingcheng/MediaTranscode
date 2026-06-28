#include "internal/graph/planner/adapters/LocalGraphPlannerAdapter.h"

#include "internal/graph/planner/local/MediaLocalPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

const char* LocalGraphPlannerAdapter::name() const noexcept
{
    return "local";
}

::media::Result<MediaGraphPlannerAdapterResult> LocalGraphPlannerAdapter::plan(
    const MediaGraph& graph,
    const MediaGraphPlannerAdapterOptions& options) const
{
    MediaLocalPlannerOptions localOptions;
    localOptions.nodeId = options.localNodeId;
    localOptions.host = options.host;
    localOptions.zone = options.zone;
    localOptions.port = options.basePort;
    localOptions.enableGpuPlanning = options.enableGpuPlanning;
    localOptions.enableMeshPlanning = options.enableMeshPlanning;
    localOptions.preferZeroCopy = options.preferZeroCopy;

    auto local = MediaLocalPlanner::plan(graph, localOptions);
    if (!local) {
        return ::media::Result<MediaGraphPlannerAdapterResult>::failure(local.error());
    }

    MediaGraphPlannerAdapterResult result;
    result.topology = std::move(local.value().topology);
    result.policy = std::move(local.value().policy);
    result.plannerResult = std::move(local.value().plannerResult);
    return ::media::Result<MediaGraphPlannerAdapterResult>::success(std::move(result));
}

MediaGraphClusterTopology LocalGraphPlannerAdapter::buildTopology(const MediaGraphPlannerAdapterOptions& options)
{
    MediaLocalPlannerOptions localOptions;
    localOptions.nodeId = options.localNodeId;
    localOptions.host = options.host;
    localOptions.zone = options.zone;
    localOptions.port = options.basePort;
    return MediaLocalPlanner::buildTopology(localOptions);
}

MediaGraphPlanningPolicy LocalGraphPlannerAdapter::buildPolicy(const MediaGraphPlannerAdapterOptions& options)
{
    MediaLocalPlannerOptions localOptions;
    localOptions.enableGpuPlanning = options.enableGpuPlanning;
    localOptions.enableMeshPlanning = options.enableMeshPlanning;
    localOptions.preferZeroCopy = options.preferZeroCopy;
    return MediaLocalPlanner::buildPolicy(localOptions);
}

} // namespace media::ffmpeg::graph
