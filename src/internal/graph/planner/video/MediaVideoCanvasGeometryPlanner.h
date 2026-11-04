#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"
#include <span>

namespace media::ffmpeg::graph {
struct MediaInputVideoStreamInfo;

struct MediaVideoCanvasGeometryPlan final {
    MediaVideoCanvasGeometry geometry;
    MediaRational sampleAspectRatio;
    // Grid ownership differs from the centered image copied onto the black canvas.
    std::vector<MediaVideoCanvasRectangle> cells;
};

class MediaVideoCanvasGeometryPlanner final {
public:
    static ::media::Result<MediaVideoCanvasGeometryPlan> plan(
        std::span<const MediaInputVideoStreamInfo> sources,
        const MediaHardwareDescriptor& output,
        MediaVideoColorRangeFact effectiveColorRange);
};
} // namespace media::ffmpeg::graph
