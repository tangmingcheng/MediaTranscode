#pragma once
#include <cstddef>
namespace media::ffmpeg::graph {
struct MediaSharedAllocationGeometry final {
    std::size_t count;
    std::size_t bytes;
    std::size_t alignment;
    friend bool operator==(const MediaSharedAllocationGeometry&,
        const MediaSharedAllocationGeometry&) = default;
};
} // namespace media::ffmpeg::graph
