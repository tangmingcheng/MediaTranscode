#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaPipelinePresetKind {
    LocalFileRemux,
    LocalFileTranscodeSkeleton,
    RealtimeRtpSkeleton
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
    static ::media::Result<MediaGraph> createLocalFileRemux(const MediaPipelinePresetOptions& options);
    static ::media::Result<MediaGraph> createLocalFileTranscodeSkeleton(const MediaPipelinePresetOptions& options);
    static ::media::Result<MediaGraph> createRealtimeRtpSkeleton(const MediaPipelinePresetOptions& options);
};

} // namespace media::ffmpeg::graph
