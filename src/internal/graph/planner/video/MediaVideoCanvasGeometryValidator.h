#pragma once
#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaVideoCanvasGeometryValidator final {
public:
    static ::media::Status validate(const MediaVideoCanvasGeometry& geometry);
};
} // namespace media::ffmpeg::graph
