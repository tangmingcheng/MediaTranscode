#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/factory/MediaRealtimeRuntimeBinding.h"

#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeExecutableGraph final {
    MediaGraph graph;
    std::vector<MediaPreparedRealtimeInputBinding> inputBindings;
    MediaRealtimeRuntimeBinding runtimeBinding;
};

} // namespace media::ffmpeg::graph
