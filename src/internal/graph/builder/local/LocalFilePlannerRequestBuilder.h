#pragma once

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class LocalFilePlannerRequestBuilder final {
public:
    static ::media::Result<MediaPipelinePlannerOptions> buildVideoPlannerOptions(
        const LocalFileTranscodeOptions& options);

private:
    LocalFilePlannerRequestBuilder() = default;
};

} // namespace media::ffmpeg::graph
