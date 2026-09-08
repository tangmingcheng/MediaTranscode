#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaRtpWirePacketComposer final {
public:
    static ::media::Result<std::vector<std::uint8_t>> compose(
        std::span<const std::uint8_t> packetizedRtp,
        std::size_t expectedPayloadOctets,
        const MediaRtpDatagramRewriteIdentity& identity,
        MediaRtpTimestamp timestamp,
        std::uint16_t sequenceNumber,
        std::size_t maximumDatagramBytes);

private:
    MediaRtpWirePacketComposer() = delete;
};

} // namespace media::ffmpeg::graph
