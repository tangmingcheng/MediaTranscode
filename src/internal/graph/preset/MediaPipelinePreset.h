#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaPipelinePresetKind {
    LocalFileTranscodeSkeleton
};

struct MediaPipelinePresetOptions {
    std::string inputUrl;
    std::string outputUrl;
    std::optional<MediaTranscodeStreamSet> streamSet;
    MediaGraphQueueParameters queues;
};

class MediaPipelinePreset final {
public:
    static ::media::Result<MediaGraph> create(MediaPipelinePresetKind kind,
                                              const MediaPipelinePresetOptions& options);

private:
    static ::media::Result<MediaGraph> createLocalFileTranscodeSkeleton(const MediaPipelinePresetOptions& options);
};

} // namespace media::ffmpeg::graph
