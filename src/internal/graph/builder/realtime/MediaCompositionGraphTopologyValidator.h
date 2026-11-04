#pragma once
#include "internal/graph/planner/realtime/MediaCompositionGraphTopologyPlanner.h"
#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/model/MediaCompositionGraphIdentity.h"
#include "internal/graph/core/MediaGraph.h"
namespace media::ffmpeg::graph {
class MediaCompositionGraphTopologyValidator final {
public:
    static ::media::Result<MediaCompositionGraphIdentity> validate(const MediaGraph& graph, const MediaCompositionGraphTopology& topology,
        std::span<const MediaEndpoint> endpoints, std::span<const std::vector<MediaNodeId>> sourceMembers,
        std::span<const MediaNodeId> outputMembers);
};
}
