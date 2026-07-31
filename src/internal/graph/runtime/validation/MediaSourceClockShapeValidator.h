#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaSourceClockShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaAvSyncRuntimeBinding& binding);

private:
    MediaSourceClockShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
