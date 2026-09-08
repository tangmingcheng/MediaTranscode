#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaWireBurstGeometry final {
    std::uint64_t datagramCount = 0;
    std::uint64_t wireBytes = 0;

    static ::media::Result<MediaWireBurstGeometry> create(
        std::uint64_t udpPayloadBytes,
        std::uint64_t payloadDatagramCount,
        std::uint64_t discreteDatagramCount,
        std::uint64_t maximumUdpPayloadBytes,
        std::uint64_t networkHeaderBytes);

    friend bool operator==(const MediaWireBurstGeometry&,
                           const MediaWireBurstGeometry&) = default;
};

} // namespace media::ffmpeg::graph
