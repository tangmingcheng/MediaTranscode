#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

struct LocalFileTranscodeOptions {
    std::string inputUrl;
    std::string outputUrl;
    std::string outputFormat;

    bool includeVideo = true;
    bool includeAudio = true;
    bool audioTranscode = false;
    bool useHardwareTransfer = false;
    bool includeMux = true;
    bool includeLifecycle = false;

    std::size_t metadataQueueCapacity = 1;
    std::size_t packetQueueCapacity = 256;
    std::size_t frameQueueCapacity = 128;
    std::size_t muxQueueCapacity = 256;
};

class LocalFileTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const LocalFileTranscodeOptions& options);

private:
    static ::media::Status validate(const LocalFileTranscodeOptions& options);
};

} // namespace media::ffmpeg::graph
