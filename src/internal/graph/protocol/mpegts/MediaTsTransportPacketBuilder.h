#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsTransportPacketBuilder final {
public:
    static ::media::Result<std::size_t> payloadPacketCount(
        std::span<const std::span<const std::uint8_t>> segments,
        bool randomAccess,
        bool discontinuity);

    static ::media::Result<std::vector<std::array<std::uint8_t, 188>>> payload(
        std::uint16_t pid,
        std::uint8_t initialPayloadContinuity,
        std::span<const std::span<const std::uint8_t>> segments,
        bool randomAccess,
        bool discontinuity,
        std::vector<std::array<std::uint8_t, 188>> workspace);

    static ::media::Result<std::array<std::uint8_t, 188>> pcrOnly(
        std::uint16_t pid,
        std::uint8_t nextPayloadContinuity,
        std::uint64_t wire27Mhz,
        bool discontinuity);
};

} // namespace media::ffmpeg::graph
