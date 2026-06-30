#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphTopologyResult {
    std::vector<MediaNodeId> order;
    std::vector<MediaNodeId> sources;
    std::vector<MediaNodeId> sinks;
    std::vector<std::vector<MediaNodeId>> levels;
};

class MediaGraphTopology final {
public:
    static ::media::Result<MediaGraphTopologyResult> build(const MediaGraph& graph);

private:
    MediaGraphTopology() = default;
};

} // namespace media::ffmpeg::graph
