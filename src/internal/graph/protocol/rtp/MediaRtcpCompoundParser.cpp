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

using CompoundResult = ::media::Result<MediaRtcpPackets>;

::media::Status packetError(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
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

::media::Status validatePacket(std::span<const uint8_t> bytes, uint8_t count, uint8_t packetType,
                         uint8_t paddingSize)
{
    const std::size_t end = bytes.size() - paddingSize;
    if (packetType == 200) {
        const std::size_t reportBytes = static_cast<std::size_t>(count) * 24;
        if (end != 28 + reportBytes) return packetError("RTCP sender report length is invalid");
        return ::media::Status::success();
    }
    if (packetType == 201) {
        const std::size_t reportBytes = static_cast<std::size_t>(count) * 24;
        if (end != 8 + reportBytes) return packetError("RTCP receiver report length is invalid");
        return ::media::Status::success();
    }
    if (packetType == 202) {
        std::size_t offset = 4;
        for (uint8_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
            if (end - offset < 4) return packetError("RTCP SDES chunk SSRC is truncated");
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
                offset += itemSize;
            }
            if (!ended) return packetError("RTCP SDES chunk has no end item");
            const std::size_t aligned = (offset + 3) & ~std::size_t(3);
            if (aligned > end || !zeroBytes(bytes, offset, aligned)) {
                return packetError("RTCP SDES alignment padding is invalid");
            }
            offset = aligned;
        }
        if (offset != end) return packetError("RTCP SDES contains trailing bytes");
        return ::media::Status::success();
    }
    if (packetType == 203) {
        std::size_t offset = 4;
        if (static_cast<std::size_t>(count) > (end - offset) / 4) {
            return packetError("RTCP BYE source list is truncated");
        }
        for (uint8_t index = 0; index < count; ++index) {
            offset += 4;
        }
        if (offset < end) {
            const std::size_t reasonSize = bytes[offset++];
            if (reasonSize > end - offset) return packetError("RTCP BYE reason is truncated");
            offset += reasonSize;
            const std::size_t alignedEnd = (offset + 3) & ~std::size_t(3);
            if (alignedEnd != end || !zeroBytes(bytes, offset, end)) {
                return packetError("RTCP BYE alignment padding is invalid");
            }
        }
        return ::media::Status::success();
    }
    return ::media::Status::success();
}

} // namespace

MediaRtcpSdesItem MediaRtcpSdesItem::read(std::span<const uint8_t> bytes) noexcept
{ return {bytes[0], bytes.subspan(2, bytes[1]), static_cast<std::size_t>(2 + bytes[1])}; }

MediaRtcpSdesChunk MediaRtcpSdesChunk::read(std::span<const uint8_t> bytes) noexcept
{
    std::size_t offset = 4;
    while (bytes[offset]) offset += 2 + bytes[offset + 1];
    return {readU32(bytes, 0), MediaRtcpWireRange<MediaRtcpSdesItem>{bytes.subspan(4, offset - 4)},
        (offset + 1 + 3) & ~std::size_t(3)};
}

MediaRtcpByeSource MediaRtcpByeSource::read(std::span<const uint8_t> bytes) noexcept
{ return {readU32(bytes, 0), 4}; }

MediaRtcpPacket MediaRtcpPacket::read(std::span<const uint8_t> bytes) noexcept
{
    const auto length = (static_cast<std::size_t>(readU16(bytes, 2)) + 1) * 4;
    const auto count = static_cast<uint8_t>(bytes[0] & 0x1f);
    const auto padding = static_cast<uint8_t>((bytes[0] & 0x20) ? bytes[length - 1] : 0);
    const auto end = length - padding;
    MediaRtcpPacket packet{MediaRtcpPacketKind::Unknown, bytes[1], count, padding,
        std::nullopt, std::nullopt, MediaRtcpWireRange<MediaRtcpSdesChunk>{},
        MediaRtcpWireRange<MediaRtcpByeSource>{}, {}, length};
    if (bytes[1] == 200) {
        packet.kind = MediaRtcpPacketKind::SenderReport;
        packet.senderReport = MediaRtcpSenderReport{readU32(bytes, 4),
            {readU32(bytes, 8), readU32(bytes, 12)}, readU32(bytes, 16), readU32(bytes, 20), readU32(bytes, 24)};
    } else if (bytes[1] == 201) {
        packet.kind = MediaRtcpPacketKind::ReceiverReport;
        packet.receiverReportSsrc = readU32(bytes, 4);
    } else if (bytes[1] == 202) {
        packet.kind = MediaRtcpPacketKind::SourceDescription;
        packet.sdesChunks = MediaRtcpWireRange<MediaRtcpSdesChunk>{bytes.subspan(4, end - 4)};
    } else if (bytes[1] == 203) {
        packet.kind = MediaRtcpPacketKind::Bye;
        packet.byeSources = MediaRtcpWireRange<MediaRtcpByeSource>{bytes.subspan(4, count * 4)};
        const auto reason = static_cast<std::size_t>(4 + count * 4);
        if (reason < end) packet.byeReason = bytes.subspan(reason + 1, bytes[reason]);
    }
    return packet;
}

::media::Result<MediaRtcpPackets> MediaRtcpCompoundParser::parse(
    std::span<const uint8_t> datagram, const MediaRtcpCompoundPolicy& policy) try
{
    if (policy.mode != MediaRtcpCompositionMode::StrictCompoundRfc3550 &&
        policy.mode != MediaRtcpCompositionMode::ReducedSizeRfc5506) {
        return compoundError("Unsupported RTCP composition mode");
    }
    if (datagram.empty()) return compoundError("RTCP compound datagram is empty");
    MediaRtcpPackets packets{datagram};
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
        auto packet = validatePacket(datagram.subspan(offset, packetSize), count,
                                  datagram[offset + 1], paddingSize);
        if (!packet) return CompoundResult::failure(packet.error());
        offset += packetSize;
    }
    const bool beginsWithReport =
        packets.front().kind == MediaRtcpPacketKind::SenderReport ||
        packets.front().kind == MediaRtcpPacketKind::ReceiverReport;
    if (policy.mode == MediaRtcpCompositionMode::StrictCompoundRfc3550 &&
        !beginsWithReport) {
        return compoundError("Strict RTCP compound packet must begin with SR or RR");
    }
    if (policy.requireCname) {
        if (!beginsWithReport) {
            return compoundError(
                "RTCP CNAME validation requires a leading SR or RR");
        }
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
catch (const std::bad_alloc&) {
    return CompoundResult::failure(::media::ErrorInfo::allocationFailed(
        "RTCP parser allocation failed"));
}

} // namespace media::ffmpeg::graph
