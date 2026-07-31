#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvCommonCoreShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaAvSyncRuntimeBinding& binding);

private:
    MediaAvCommonCoreShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
