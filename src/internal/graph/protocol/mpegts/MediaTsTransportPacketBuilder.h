#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaTsTransportPacketBuilder final {
public:
    // pcrOnly produces one fixed-size transport packet.
    static constexpr std::size_t PcrOnlyPacketCount = 1;

    static ::media::Result<std::size_t> payloadPacketCount(
        std::size_t logicalSize,
        bool randomAccess,
        bool discontinuity);

    static ::media::Result<std::size_t> payloadPacketCount(
        std::span<const std::span<const std::uint8_t>> segments,
        bool randomAccess,
        bool discontinuity);

    static ::media::Status payload(
        std::uint16_t pid,
        std::uint8_t initialPayloadContinuity,
        std::span<const std::span<const std::uint8_t>> segments,
        bool randomAccess,
        bool discontinuity,
        std::span<std::array<std::uint8_t, 188>> packets);

    static ::media::Result<std::array<std::uint8_t, 188>> pcrOnly(
        std::uint16_t pid,
        std::uint8_t nextPayloadContinuity,
        std::uint64_t wire27Mhz,
        bool discontinuity);

    // An earlier adaptation-only packet consumed the discontinuity. The
    // unpublished payload keeps its existing adaptation stuffing and offsets.
    static ::media::Status clearPreparedDiscontinuity(
        std::array<std::uint8_t, 188>& packet);
};

} // namespace media::ffmpeg::graph
