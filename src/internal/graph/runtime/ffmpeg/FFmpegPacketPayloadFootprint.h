#pragma once

extern "C" {
#include <libavcodec/packet.h>
}

#include <cstdint>
#include <limits>
#include <optional>

namespace media::ffmpeg::graph {

inline std::optional<std::uint64_t> ffmpegPacketPayloadFootprintBytes(
    const AVPacket& packet) noexcept
{
    if (packet.size < 0 ||
        (packet.size > 0 && !packet.data) ||
        packet.side_data_elems < 0 ||
        (packet.side_data_elems > 0 && !packet.side_data)) {
        return std::nullopt;
    }
    std::uint64_t bytes = static_cast<std::uint64_t>(packet.size);
    for (int index = 0; index < packet.side_data_elems; ++index) {
        const auto& sideData = packet.side_data[index];
        if (sideData.size > 0 && !sideData.data) return std::nullopt;
        if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
            if (sideData.size >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max())) {
                return std::nullopt;
            }
        }
        const auto sideBytes = static_cast<std::uint64_t>(sideData.size);
        if (sideBytes > std::numeric_limits<std::uint64_t>::max() - bytes) {
            return std::nullopt;
        }
        bytes += sideBytes;
    }
    return bytes == 0 ? std::nullopt
                      : std::optional<std::uint64_t>(bytes);
}

} // namespace media::ffmpeg::graph
