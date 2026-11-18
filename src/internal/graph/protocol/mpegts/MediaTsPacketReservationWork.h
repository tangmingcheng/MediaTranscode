#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

// Exact published protocol work. Unreserved AU tails and RTP control are separate.
struct MediaTsPacketReservationWork final {
    std::uint64_t generation;
    MediaRunningTime release;
    MediaRunningTime deadline;
    MediaRunningTime completionLimit;
    std::size_t protocolBegin;
    std::size_t datagramCount;
    std::size_t packetCount;
    std::size_t maximumPacketsPerDatagram;
};

} // namespace media::ffmpeg::graph
