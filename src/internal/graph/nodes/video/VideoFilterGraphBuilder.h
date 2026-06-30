#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

#include <string>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

struct VideoFilterGraphBuildRequest {
    const MediaNodeOptions* options = nullptr;
    const AVFrame* firstFrame = nullptr;
    AVRational inputTimeBase { 0, 1 };
    AVRational inputFrameRate { 0, 1 };
    AVRational sampleAspectRatio { 1, 1 };
};

struct VideoFilterGraphBuildResult {
    ::media::ffmpeg::FilterGraphPtr graph;
    AVFilterContext* bufferSource = nullptr;
    AVFilterContext* bufferSink = nullptr;
    AVRational sinkTimeBase { 0, 1 };
    std::string plannerFilter;
    std::string filterDescription;
    bool hardwareSource = false;
};

class VideoFilterGraphBuilder final {
public:
    static ::media::Result<VideoFilterGraphBuildResult> build(const VideoFilterGraphBuildRequest& request);

private:
    VideoFilterGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
