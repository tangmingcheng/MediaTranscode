#pragma once

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaNodeId.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class LocalFileAudioBranchBuilder final {
public:
    static ::media::Result<bool> buildIfPlanned(MediaGraph& graph,
                                                const LocalFileTranscodeOptions& options,
                                                MediaNodeId fileInput,
                                                MediaNodeId split,
                                                MediaNodeId mux);

private:
    LocalFileAudioBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
