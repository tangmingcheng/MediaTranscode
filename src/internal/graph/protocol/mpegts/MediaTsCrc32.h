#pragma once

#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaTsCrc32 final {
public:
    static std::uint32_t compute(std::span<const std::uint8_t> bytes) noexcept;
};

} // namespace media::ffmpeg::graph
