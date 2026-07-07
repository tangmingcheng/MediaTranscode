#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaPipelinePresetKind {
    LocalFileTranscodeSkeleton
};

struct MediaPipelinePresetOptions {
    std::string inputUrl;
    std::string outputUrl;
    std::string outputFormat;
    bool includeAudio = true;
    bool includeVideo = true;
};

class MediaPipelinePreset final {
public:
    static ::media::Result<MediaGraph> create(MediaPipelinePresetKind kind,
                                              const MediaPipelinePresetOptions& options);

private:
    static ::media::Result<MediaGraph> createLocalFileTranscodeSkeleton(const MediaPipelinePresetOptions& options);
};

} // namespace media::ffmpeg::graph
