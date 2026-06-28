#include "internal/graph/planner/MediaGraphGpuPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaGraphGpuPlannerResult> MediaGraphGpuPlanner::plan(
    const MediaGraph& graph,
    const MediaGraphPlanningPolicy& policy)
{
    MediaGraphGpuPlannerResult result;

    if (!policy.enableGpuPlanning) {
        result.report.info("gpu-planner", "GPU planning disabled");
        return ::media::Result<MediaGraphGpuPlannerResult>::success(std::move(result));
    }

    for (const MediaNode& node : graph.nodes()) {
        if (!isGpuCandidate(node.kind)) {
            continue;
        }

        MediaGpuGraphCommand command;
        command.kind = commandKindFor(node.kind);
        command.nodeId = node.id;
        command.deviceKind = MediaHardwareDeviceKind::Unknown;
        command.name = node.name;
        result.commands.commands.push_back(std::move(command));
        result.report.info("gpu-planner", "planned GPU command for node: " + node.name);
    }

    if (result.commands.commands.empty()) {
        result.report.info("gpu-planner", "no GPU command candidates detected");
    }

    return ::media::Result<MediaGraphGpuPlannerResult>::success(std::move(result));
}

bool MediaGraphGpuPlanner::isGpuCandidate(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
        return true;
    default:
        return false;
    }
}

MediaGpuGraphCommandKind MediaGraphGpuPlanner::commandKindFor(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::HardwareTransfer:
        return MediaGpuGraphCommandKind::Map;
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::VideoFilter:
        return MediaGpuGraphCommandKind::Kernel;
    default:
        return MediaGpuGraphCommandKind::Unknown;
    }
}

} // namespace media::ffmpeg::graph
