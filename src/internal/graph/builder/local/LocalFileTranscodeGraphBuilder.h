#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct LocalFileTranscodeOptions {
    std::string inputUrl;
    std::string outputUrl;
    MediaTranscodeParameterSet parameters;
};

class LocalFileTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const LocalFileTranscodeOptions& options);

private:
    static ::media::Status validate(const LocalFileTranscodeOptions& options);
};

} // namespace media::ffmpeg::graph
