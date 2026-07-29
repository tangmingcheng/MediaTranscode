#pragma once

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaSourceClockReadiness {
    Acquiring = 0,
    Locked = 1,
    ReacquireRequired = 2,
    Degraded = 3
};

struct MediaPacketSourceTiming final {
    std::optional<std::int64_t> presentationNs;
    std::optional<std::int64_t> decodeNs;
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;
    std::optional<std::int64_t> durationNs = std::nullopt;

    bool operator==(const MediaPacketSourceTiming&) const = default;
};

} // namespace media::ffmpeg::graph
