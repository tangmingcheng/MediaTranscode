#pragma once

#include <cstddef>

namespace media::ffmpeg::graph {

enum class MediaBackpressureMode {
    None,
    Block,
    Drop,
    Adaptive,
    Abort
};

struct MediaBackpressurePolicy {
    MediaBackpressureMode mode = MediaBackpressureMode::Block;

    std::size_t lowWatermark = 0;
    std::size_t highWatermark = 0;
    std::size_t criticalWatermark = 0;

    bool propagateUpstream = true;
    bool reportMetrics = true;
    bool realtimePriority = false;

    constexpr bool operator==(
        const MediaBackpressurePolicy&) const noexcept = default;

    constexpr bool enabled() const noexcept
    {
        return mode != MediaBackpressureMode::None;
    }

    constexpr bool hasWatermarks() const noexcept
    {
        return highWatermark > 0 || criticalWatermark > 0;
    }
};

} // namespace media::ffmpeg::graph
