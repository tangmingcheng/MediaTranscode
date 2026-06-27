#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/MediaGraphBuilder.h"

namespace media::ffmpeg::graph {

struct LocalFileTranscodeOptions {
    bool includeVideo = true;
    bool includeAudio = true;
    bool audioTranscode = false;
    bool useHardwareTransfer = true;
    bool includeMux = true;
    bool includeLifecycle = false;
};

class LocalFileTranscodeGraphBuilder {
public:
    static MediaGraph build(const LocalFileTranscodeOptions& opt = {});
};

} // namespace media::ffmpeg::graph
