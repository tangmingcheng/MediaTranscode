#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

namespace media::ffmpeg::graph {

class MediaAvRuntimeRegistrationValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph, const MediaAvSyncRuntimeBinding& binding);
};

} // namespace media::ffmpeg::graph
