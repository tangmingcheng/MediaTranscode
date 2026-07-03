#pragma once

#include "internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaVideoTranscodeOptionApplier final {
public:
    static ::media::Result<void> applyUserOptions(MediaGraph& graph,
                                                  const MediaVideoTranscodeBranchNodes& nodes,
                                                  const MediaVideoTranscodeParameters& video);

private:
    MediaVideoTranscodeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
