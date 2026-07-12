#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

struct AacAuHeader final {
    std::size_t size;
    uint64_t index;
};

::media::Result<std::vector<AacAuHeader>> parseAuHeaders(const std::vector<uint8_t>& payload)
{
    if (payload.size() < 4) return ::media::Result<std::vector<AacAuHeader>>::failure(
        ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC RTP payload is truncated"));
    const std::size_t headerBits = (static_cast<std::size_t>(payload[0]) << 8) | payload[1];
    if (headerBits == 0 || (headerBits % 16) != 0) return ::media::Result<std::vector<AacAuHeader>>::failure(
        ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC AU headers must use complete planned 16-bit headers"));
    const std::size_t headerCount = headerBits / 16;
    const std::size_t headerBytes = headerBits / 8;
    if (headerBytes > payload.size() - 2) return ::media::Result<std::vector<AacAuHeader>>::failure(
        ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC AU header section is truncated"));

    std::vector<AacAuHeader> headers;
    headers.reserve(headerCount);
    for (std::size_t index = 0; index < headerCount; ++index) {
        const std::size_t offset = 2 + index * 2;
        const uint16_t bits = static_cast<uint16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1]);
        const std::size_t auSize = bits >> 3;
        const uint8_t indexField = static_cast<uint8_t>(bits & 0x07);
        if (auSize == 0) return ::media::Result<std::vector<AacAuHeader>>::failure(
            ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC AU size is zero"));
        uint64_t auIndex = indexField;
        if (!headers.empty()) {
            const uint64_t increment = static_cast<uint64_t>(indexField) + 1;
            if (headers.back().index > std::numeric_limits<uint64_t>::max() - increment) return ::media::Result<std::vector<AacAuHeader>>::failure(
                ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC expanded AU index overflows"));
            auIndex = headers.back().index + increment;
        }
        headers.push_back(AacAuHeader{auSize, auIndex});
    }
    return ::media::Result<std::vector<AacAuHeader>>::success(std::move(headers));
}

} // namespace

MediaAacRtpDepacketizer::MediaAacRtpDepacketizer(MediaRtpDepacketizerConfig config)
    : m_config(std::move(config))
{
}

::media::Result<MediaRtpDepacketizerResult> MediaAacRtpDepacketizer::push(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_config.payloadType) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC RTP payload type changed"));
    auto headers = parseAuHeaders(packet.payload);
    if (!headers) return ::media::Result<MediaRtpDepacketizerResult>::failure(headers.error());
    const std::size_t payloadOffset = 2 + headers.value().size() * 2;
    std::size_t totalAuBytes = 0;
    for (const AacAuHeader& header : headers.value()) {
        if (header.size > std::numeric_limits<std::size_t>::max() - totalAuBytes) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC aggregate size overflow"));
        totalAuBytes += header.size;
    }
    if (payloadOffset > packet.payload.size() || totalAuBytes != packet.payload.size() - payloadOffset) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC aggregate AU sizes do not partition payload"));
    }

    MediaRtpDepacketizerResult result;
    result.accessUnits.reserve(headers.value().size());
    std::size_t offset = payloadOffset;
    const uint64_t firstIndex = headers.value().front().index;
    for (const AacAuHeader& header : headers.value()) {
        std::vector<uint8_t> bytes(packet.payload.begin() + offset, packet.payload.begin() + offset + header.size);
        const uint64_t relativeIndex = header.index - firstIndex;
        const uint64_t duration = static_cast<uint64_t>(m_config.accessUnitDurationRtpTicks);
        if (relativeIndex != 0 && duration > std::numeric_limits<uint64_t>::max() / relativeIndex) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC AU timestamp offset overflows"));
        const uint64_t timestampOffset = relativeIndex * duration;
        const uint32_t timestamp = packet.timestamp + static_cast<uint32_t>(timestampOffset);
        auto unit = makeRtpAccessUnit(std::move(bytes), timestamp, m_config.clockRate,
                                     m_config.accessUnitDurationRtpTicks, true);
        if (!unit) return ::media::Result<MediaRtpDepacketizerResult>::failure(unit.error());
        result.accessUnits.push_back(std::move(unit).value());
        offset += header.size;
    }
    return ::media::Result<MediaRtpDepacketizerResult>::success(std::move(result));
}

void MediaAacRtpDepacketizer::discontinuity(MediaRtpDiscontinuityReason) noexcept
{
}

} // namespace media::ffmpeg::graph
