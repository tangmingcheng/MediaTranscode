#pragma once

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaPacketSourceTiming final {
    std::optional<std::int64_t> presentationNs;
    std::optional<std::int64_t> decodeNs;
    std::uint64_t generation = 0;

    bool operator==(const MediaPacketSourceTiming&) const = default;
};

} // namespace media::ffmpeg::graph
