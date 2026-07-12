#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"

#include <cstddef>

namespace media::ffmpeg::graph {
namespace {

uint16_t readU16(std::span<const uint8_t> bytes, std::size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) |
                                 static_cast<uint16_t>(bytes[offset + 1]));
}

uint32_t readU32(std::span<const uint8_t> bytes, std::size_t offset)
{
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

::media::Result<MediaRtpPacket> malformed(const char* message)
{
    return ::media::Result<MediaRtpPacket>::failure(::media::ErrorInfo::invalidArgument(message));
}

} // namespace

::media::Result<MediaRtpPacket> MediaRtpPacketParser::parse(std::span<const uint8_t> datagram)
{
    constexpr std::size_t FixedHeaderSize = 12;
    if (datagram.size() < FixedHeaderSize) return malformed("RTP datagram is shorter than fixed header");

    const uint8_t first = datagram[0];
    const uint8_t version = static_cast<uint8_t>(first >> 6);
    if (version != 2) return malformed("RTP version must be 2");
    const bool hasPadding = (first & 0x20) != 0;
    const bool hasExtension = (first & 0x10) != 0;
    const std::size_t csrcCount = first & 0x0F;
    if (csrcCount > (datagram.size() - FixedHeaderSize) / 4) {
        return malformed("RTP CSRC list is truncated");
    }

    MediaRtpPacket packet{
        version,
        (datagram[1] & 0x80) != 0,
        static_cast<uint8_t>(datagram[1] & 0x7F),
        readU16(datagram, 2),
        readU32(datagram, 4),
        readU32(datagram, 8),
        {},
        std::nullopt,
        0,
        {}
    };

    std::size_t offset = FixedHeaderSize;
    packet.csrcs.reserve(csrcCount);
    for (std::size_t index = 0; index < csrcCount; ++index) {
        packet.csrcs.push_back(readU32(datagram, offset));
        offset += 4;
    }

    if (hasExtension) {
        if (datagram.size() - offset < 4) return malformed("RTP extension header is truncated");
        const uint16_t profile = readU16(datagram, offset);
        const std::size_t words = readU16(datagram, offset + 2);
        offset += 4;
        if (words > (datagram.size() - offset) / 4) return malformed("RTP extension data is truncated");
        const std::size_t size = words * 4;
        packet.extension = MediaRtpHeaderExtension{
            profile, std::vector<uint8_t>(datagram.begin() + offset, datagram.begin() + offset + size)};
        offset += size;
    }

    std::size_t payloadEnd = datagram.size();
    if (hasPadding) {
        const uint8_t paddingSize = datagram.back();
        if (paddingSize == 0 || static_cast<std::size_t>(paddingSize) > payloadEnd - offset) {
            return malformed("RTP padding length is invalid");
        }
        packet.paddingSize = paddingSize;
        payloadEnd -= paddingSize;
    }
    packet.payload.assign(datagram.begin() + offset, datagram.begin() + payloadEnd);
    return ::media::Result<MediaRtpPacket>::success(std::move(packet));
}

} // namespace media::ffmpeg::graph
