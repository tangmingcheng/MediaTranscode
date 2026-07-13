#pragma once

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaSourceClockReadiness {
    Acquiring,
    Locked,
    ReacquireRequired
};

struct MediaPacketSourceTiming final {
    std::optional<std::int64_t> presentationNs;
    std::optional<std::int64_t> decodeNs;
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;

    bool operator==(const MediaPacketSourceTiming&) const = default;
};

} // namespace media::ffmpeg::graph
