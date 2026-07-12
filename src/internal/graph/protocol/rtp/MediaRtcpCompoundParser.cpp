#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"

#include <cstddef>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

uint16_t readU16(std::span<const uint8_t> bytes, std::size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

uint32_t readU32(std::span<const uint8_t> bytes, std::size_t offset)
{
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           bytes[offset + 3];
}

using PacketResult = ::media::Result<MediaRtcpPacket>;
using CompoundResult = ::media::Result<std::vector<MediaRtcpPacket>>;

PacketResult packetError(const char* message)
{
    return PacketResult::failure(::media::ErrorInfo::invalidArgument(message));
}

CompoundResult compoundError(const char* message)
{
    return CompoundResult::failure(::media::ErrorInfo::invalidArgument(message));
}

bool zeroBytes(std::span<const uint8_t> bytes, std::size_t begin, std::size_t end)
{
    for (std::size_t offset = begin; offset < end; ++offset) {
        if (bytes[offset] != 0) return false;
    }
    return true;
}

PacketResult parsePacket(std::span<const uint8_t> bytes, uint8_t count, uint8_t packetType,
                         uint8_t paddingSize)
{
    const std::size_t end = bytes.size() - paddingSize;
    MediaRtcpPacket packet{MediaRtcpPacketKind::Unknown, packetType, count, paddingSize,
                           std::nullopt, std::nullopt, {}, {}, {}};
    if (packetType == 200) {
        const std::size_t reportBytes = static_cast<std::size_t>(count) * 24;
        if (end != 28 + reportBytes) return packetError("RTCP sender report length is invalid");
        packet.kind = MediaRtcpPacketKind::SenderReport;
        packet.senderReport = MediaRtcpSenderReport{
            readU32(bytes, 4),
            MediaRtcpNtpTimestamp{readU32(bytes, 8), readU32(bytes, 12)},
            readU32(bytes, 16), readU32(bytes, 20), readU32(bytes, 24)};
        return PacketResult::success(std::move(packet));
    }
    if (packetType == 201) {
        const std::size_t reportBytes = static_cast<std::size_t>(count) * 24;
        if (end != 8 + reportBytes) return packetError("RTCP receiver report length is invalid");
        packet.kind = MediaRtcpPacketKind::ReceiverReport;
        packet.receiverReportSsrc = readU32(bytes, 4);
        return PacketResult::success(std::move(packet));
    }
    if (packetType == 202) {
        packet.kind = MediaRtcpPacketKind::SourceDescription;
        std::size_t offset = 4;
        packet.sdesChunks.reserve(count);
        for (uint8_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
            if (end - offset < 4) return packetError("RTCP SDES chunk SSRC is truncated");
            MediaRtcpSdesChunk chunk{readU32(bytes, offset), {}};
            offset += 4;
            bool ended = false;
            while (offset < end) {
                const uint8_t type = bytes[offset++];
                if (type == 0) {
                    ended = true;
                    break;
                }
                if (offset >= end) return packetError("RTCP SDES item length is truncated");
                const std::size_t itemSize = bytes[offset++];
                if (itemSize > end - offset) return packetError("RTCP SDES item value is truncated");
                chunk.items.push_back(MediaRtcpSdesItem{
                    type, std::vector<uint8_t>(bytes.begin() + offset, bytes.begin() + offset + itemSize)});
                offset += itemSize;
            }
            if (!ended) return packetError("RTCP SDES chunk has no end item");
            const std::size_t aligned = (offset + 3) & ~std::size_t(3);
            if (aligned > end || !zeroBytes(bytes, offset, aligned)) {
                return packetError("RTCP SDES alignment padding is invalid");
            }
            offset = aligned;
            packet.sdesChunks.push_back(std::move(chunk));
        }
        if (offset != end) return packetError("RTCP SDES contains trailing bytes");
        return PacketResult::success(std::move(packet));
    }
    if (packetType == 203) {
        packet.kind = MediaRtcpPacketKind::Bye;
        std::size_t offset = 4;
        if (static_cast<std::size_t>(count) > (end - offset) / 4) {
            return packetError("RTCP BYE source list is truncated");
        }
        packet.byeSources.reserve(count);
        for (uint8_t index = 0; index < count; ++index) {
            packet.byeSources.push_back(readU32(bytes, offset));
            offset += 4;
        }
        if (offset < end) {
            const std::size_t reasonSize = bytes[offset++];
            if (reasonSize > end - offset) return packetError("RTCP BYE reason is truncated");
            packet.byeReason.assign(bytes.begin() + offset, bytes.begin() + offset + reasonSize);
            offset += reasonSize;
            const std::size_t alignedEnd = (offset + 3) & ~std::size_t(3);
            if (alignedEnd != end || !zeroBytes(bytes, offset, end)) {
                return packetError("RTCP BYE alignment padding is invalid");
            }
        }
        return PacketResult::success(std::move(packet));
    }
    return PacketResult::success(std::move(packet));
}

} // namespace

::media::Result<std::vector<MediaRtcpPacket>> MediaRtcpCompoundParser::parse(
    std::span<const uint8_t> datagram, const MediaRtcpCompoundPolicy& policy)
{
    if (policy.mode != MediaRtcpCompositionMode::StrictCompoundRfc3550) {
        return compoundError("Unsupported RTCP composition mode");
    }
    if (datagram.empty()) return compoundError("RTCP compound datagram is empty");
    std::vector<MediaRtcpPacket> packets;
    std::size_t offset = 0;
    while (offset < datagram.size()) {
        if (datagram.size() - offset < 4) return compoundError("RTCP packet header is truncated");
        const uint8_t first = datagram[offset];
        if ((first >> 6) != 2) return compoundError("RTCP version must be 2");
        const bool padded = (first & 0x20) != 0;
        const uint8_t count = first & 0x1F;
        const std::size_t words = static_cast<std::size_t>(readU16(datagram, offset + 2)) + 1;
        if (words > std::numeric_limits<std::size_t>::max() / 4) {
            return compoundError("RTCP packet length overflows");
        }
        const std::size_t packetSize = words * 4;
        if (packetSize < 4 || packetSize > datagram.size() - offset) {
            return compoundError("RTCP packet length exceeds datagram");
        }
        const bool finalPacket = packetSize == datagram.size() - offset;
        if (padded && !finalPacket) return compoundError("Only final RTCP packet may be padded");
        uint8_t paddingSize = 0;
        if (padded) {
            paddingSize = datagram[offset + packetSize - 1];
            if (paddingSize == 0 || paddingSize > packetSize - 4) {
                return compoundError("RTCP padding length is invalid");
            }
        }
        auto packet = parsePacket(datagram.subspan(offset, packetSize), count,
                                  datagram[offset + 1], paddingSize);
        if (!packet) return CompoundResult::failure(packet.error());
        packets.push_back(std::move(packet.value()));
        offset += packetSize;
    }
    if (packets.front().kind != MediaRtcpPacketKind::SenderReport &&
        packets.front().kind != MediaRtcpPacketKind::ReceiverReport) {
        return compoundError("Strict RTCP compound packet must begin with SR or RR");
    }
    if (policy.requireCname) {
        const uint32_t source = packets.front().kind == MediaRtcpPacketKind::SenderReport
            ? packets.front().senderReport->ssrc
            : *packets.front().receiverReportSsrc;
        bool matchingCname = false;
        for (const MediaRtcpPacket& packet : packets) {
            for (const MediaRtcpSdesChunk& chunk : packet.sdesChunks) {
                if (chunk.ssrc != source) continue;
                for (const MediaRtcpSdesItem& item : chunk.items) {
                    if (item.type == 1 && !item.value.empty()) matchingCname = true;
                }
            }
        }
        if (!matchingCname) return compoundError("Strict RTCP compound packet requires matching non-empty CNAME");
    }
    return CompoundResult::success(std::move(packets));
}

} // namespace media::ffmpeg::graph
