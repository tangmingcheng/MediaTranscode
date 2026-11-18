#pragma once
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"
#include "internal/graph/time/MediaRunningTime.h"
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {
struct MediaMpegTsDatagramView final {
    std::span<const std::uint8_t> completeTsPackets;
    MediaRunningTime canonicalRelease;
    MediaWireMediaBoundary mediaBoundary;
};
} // namespace media::ffmpeg::graph
