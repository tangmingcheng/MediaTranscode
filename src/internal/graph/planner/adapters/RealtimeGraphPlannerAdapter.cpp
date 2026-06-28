#include "internal/graph/planner/adapters/RealtimeGraphPlannerAdapter.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

const char* RealtimeGraphPlannerAdapter::name() const noexcept
{
    return "realtime";
}

::media::Result<MediaGraphPlannerAdapterResult> RealtimeGraphPlannerAdapter::plan(
    const MediaGraph& graph,
    const MediaGraphPlannerAdapterOptions& options) const
{
    MediaRealtimePlannerOptions realtimeOptions;
    realtimeOptions.edgeNodeId = options.edgeNodeId;
    realtimeOptions.workerNodeId = options.workerNodeId;
    realtimeOptions.host = options.host;
    realtimeOptions.zone = options.zone;
    realtimeOptions.basePort = options.basePort;
    realtimeOptions.enableGpuPlanning = options.enableGpuPlanning;
    realtimeOptions.enableMeshPlanning = options.enableMeshPlanning;
    realtimeOptions.preferZeroCopy = options.preferZeroCopy;

    auto realtime = MediaRealtimePlanner::plan(graph, realtimeOptions);
    if (!realtime) {
        return ::media::Result<MediaGraphPlannerAdapterResult>::failure(realtime.error());
    }

    MediaGraphPlannerAdapterResult result;
    result.topology = std::move(realtime.value().topology);
    result.policy = std::move(realtime.value().policy);
    result.plannerResult = std::move(realtime.value().plannerResult);
    return ::media::Result<MediaGraphPlannerAdapterResult>::success(std::move(result));
}

MediaGraphClusterTopology RealtimeGraphPlannerAdapter::buildTopology(const MediaGraphPlannerAdapterOptions& options)
{
    MediaRealtimePlannerOptions realtimeOptions;
    realtimeOptions.edgeNodeId = options.edgeNodeId;
    realtimeOptions.workerNodeId = options.workerNodeId;
    realtimeOptions.host = options.host;
    realtimeOptions.zone = options.zone;
    realtimeOptions.basePort = options.basePort;
    return MediaRealtimePlanner::buildTopology(realtimeOptions);
}

MediaGraphPlanningPolicy RealtimeGraphPlannerAdapter::buildPolicy(const MediaGraphPlannerAdapterOptions& options)
{
    MediaRealtimePlannerOptions realtimeOptions;
    realtimeOptions.enableGpuPlanning = options.enableGpuPlanning;
    realtimeOptions.enableMeshPlanning = options.enableMeshPlanning;
    realtimeOptions.preferZeroCopy = options.preferZeroCopy;
    return MediaRealtimePlanner::buildPolicy(realtimeOptions);
}

} // namespace media::ffmpeg::graph
