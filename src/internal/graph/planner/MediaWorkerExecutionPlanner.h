#pragma once
#include "internal/graph/model/MediaWorkerExecutionPlan.h"
#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"
#include <span>
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
namespace media::ffmpeg::graph {
class MediaWorkerExecutionPlanner final {
public:
    static ::media::Result<MediaFixedCapacityStorage<MediaWorkerExecutionPlan>> plan(const MediaGraph& graph,
        std::span<const MediaNodeId> nodes);
};
}
