#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpHeaderExtension final {
    uint16_t profile;
    std::vector<uint8_t> data;
};

struct MediaRtpPacket final {
    uint8_t version;
    bool marker;
    uint8_t payloadType;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
    std::vector<uint32_t> csrcs;
    std::optional<MediaRtpHeaderExtension> extension;
    uint8_t paddingSize;
    std::vector<uint8_t> payload;
};

class MediaRtpPacketParser final {
public:
    static ::media::Result<MediaRtpPacket> parse(std::span<const uint8_t> datagram);

private:
    MediaRtpPacketParser() = delete;
};

} // namespace media::ffmpeg::graph
