#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaVideoRtpPayloadPacket final {
    std::vector<std::uint8_t> payload;
    bool marker = false;
};

class MediaDeterministicVideoRtpPacketizer final {
public:
    static ::media::Result<std::uint64_t> maximumDatagramsPerAccessUnit(
        std::uint64_t maximumAccessUnitBytes,
        MediaAnnexBCodec codec,
        std::size_t maximumRtpPayloadBytes);

    static ::media::Result<std::vector<MediaVideoRtpPayloadPacket>> packetize(
        std::span<const std::uint8_t> accessUnit,
        MediaAnnexBCodec codec,
        const MediaEncodedPacketLayout& layout,
        std::size_t maximumRtpPayloadBytes);

private:
    MediaDeterministicVideoRtpPacketizer() = delete;
};

} // namespace media::ffmpeg::graph
