#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"

#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeExecutableGraph final {
    MediaGraph graph;
    std::vector<MediaPreparedRealtimeInputBinding> inputBindings;
};

} // namespace media::ffmpeg::graph
