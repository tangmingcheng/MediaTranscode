#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaProtocolGenerationStoragePlan.h"
#include "media_transcode/Result.h"
#include <span>

namespace media::ffmpeg::graph {
class MediaProtocolGenerationStoragePlanner final {
public:
    static ::media::Result<MediaProtocolGenerationStoragePlan> plan(
        const MediaGraph& graph, std::span<const MediaNodeId> selectedNodes);
};
} // namespace media::ffmpeg::graph
