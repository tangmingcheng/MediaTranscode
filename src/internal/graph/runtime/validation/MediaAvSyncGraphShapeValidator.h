#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncGraphShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaAvSyncRuntimeBinding& binding);
    static ::media::Status validateAbsent(
        const MediaGraph& graph);

private:
    MediaAvSyncGraphShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
