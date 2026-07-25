#include "internal/graph/protocol/mpegts/MediaTsCrc32.h"

namespace media::ffmpeg::graph {

std::uint32_t MediaTsCrc32::compute(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80000000U) != 0
                ? (crc << 1) ^ 0x04C11DB7U
                : crc << 1;
        }
    }
    return crc;
}

} // namespace media::ffmpeg::graph
