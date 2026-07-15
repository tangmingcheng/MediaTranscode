#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeExecutableGraph final {
    MediaGraph graph;
    std::vector<MediaPreparedRealtimeInputBinding> inputBindings;
    std::optional<MediaAvSyncRuntimeBinding> avSyncBinding;
};

} // namespace media::ffmpeg::graph
