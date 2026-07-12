#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRtcpPacketKind {
    SenderReport,
    SourceDescription,
    Bye,
    Unknown
};

struct MediaRtcpSenderReport final {
    uint32_t ssrc;
    MediaRtcpNtpTimestamp ntp;
    uint32_t rtpTimestamp;
    uint32_t senderPacketCount;
    uint32_t senderOctetCount;
};

struct MediaRtcpSdesItem final {
    uint8_t type;
    std::vector<uint8_t> value;
};

struct MediaRtcpSdesChunk final {
    uint32_t ssrc;
    std::vector<MediaRtcpSdesItem> items;
};

struct MediaRtcpPacket final {
    MediaRtcpPacketKind kind;
    uint8_t packetType;
    uint8_t count;
    uint8_t paddingSize;
    std::optional<MediaRtcpSenderReport> senderReport;
    std::vector<MediaRtcpSdesChunk> sdesChunks;
    std::vector<uint32_t> byeSources;
    std::vector<uint8_t> byeReason;
};

class MediaRtcpCompoundParser final {
public:
    static ::media::Result<std::vector<MediaRtcpPacket>> parse(std::span<const uint8_t> datagram);

private:
    MediaRtcpCompoundParser() = delete;
};

} // namespace media::ffmpeg::graph
