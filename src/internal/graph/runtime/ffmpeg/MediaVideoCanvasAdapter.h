#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"
#include <memory>

namespace media::ffmpeg::graph {

// Synchronous platform operation: success means all submitted work completed.
class MediaVideoCanvasAdapter {
public:
    virtual ~MediaVideoCanvasAdapter() = default;
    virtual ::media::Status upload(const AVFrame& source, AVFrame& target) = 0;
    virtual ::media::Status download(const AVFrame& source, AVFrame& target) = 0;
    virtual ::media::Status copy(const AVFrame& source, AVFrame& target,
        const MediaVideoCanvasRectangle& targetRectangle) = 0;
    virtual ::media::Status validate(const AVFrame& source, const AVFrame& target,
        const MediaVideoCanvasRectangle& targetRectangle) = 0;
    static ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> create(AVBufferRef* frames);
};

::media::Result<MediaVideoCanvasAllocation> readMediaVideoCanvasAllocation(const AVFrame& frame);

} // namespace media::ffmpeg::graph
