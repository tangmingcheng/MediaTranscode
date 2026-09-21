#pragma once

#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaVideoCanvasRectangle final {
    int x;
    int y;
    int width;
    int height;
};

// Allocation readback from a frame obtained from the prepared encoder pool.
struct MediaVideoCanvasAllocation final {
    std::uint64_t surfaceBytes;
    std::uint64_t stagingBytes;
};

struct MediaVideoCanvasPlan final {
    AVPixelFormat hardwareFormat;
    AVPixelFormat softwareFormat;
    int width;
    int height;
    MediaVideoColorRangeFact effectiveColorRange;
    std::size_t surfaceCount;
    std::uint64_t maximumSurfaceBytes;
    std::uint64_t maximumStagingBytes;
    std::size_t maximumHeaderCount;
    std::vector<MediaVideoCanvasRectangle> tiles;
};

} // namespace media::ffmpeg::graph
