#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validH264NalHeader(std::uint8_t header) noexcept
{
    const std::uint8_t type = header & 0x1f;
    return (header & 0x80) == 0 && type >= 1 && type <= 23;
}

bool validHevcNalHeader(const std::uint8_t* data) noexcept
{
    return (data[0] & 0x80) == 0 && (data[1] & 0x07) != 0;
}

::media::ErrorInfo invalidPayload(const std::string& message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

MediaRtpNalUnit::MediaRtpNalUnit(std::span<const std::uint8_t> bytes)
    : m_borrowed(bytes)
{
}

MediaRtpNalUnit::MediaRtpNalUnit(std::vector<std::uint8_t> bytes)
    : m_owned(std::move(bytes)), m_ownsBytes(true)
{
}

MediaRtpNalUnit MediaRtpNalUnit::borrowed(std::span<const std::uint8_t> bytes)
{
    return MediaRtpNalUnit(bytes);
}

MediaRtpNalUnit MediaRtpNalUnit::owned(std::vector<std::uint8_t> bytes)
{
    return MediaRtpNalUnit(std::move(bytes));
}

std::span<const std::uint8_t> MediaRtpNalUnit::bytes() const noexcept
{
    return m_ownsBytes ? std::span<const std::uint8_t>(m_owned) : m_borrowed;
}

MediaH264RtpNalUnitParser::MediaH264RtpNalUnitParser(
    std::uint8_t payloadType) noexcept
    : m_payloadType(payloadType)
{
}

::media::Result<MediaRtpNalUnitBatch> MediaH264RtpNalUnitParser::push(
    const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_payloadType || packet.payload.empty()) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 RTP packet has invalid payload identity or empty payload"));
    }
    if ((packet.payload[0] & 0x80) != 0) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 RTP NAL forbidden_zero_bit is set"));
    }

    MediaRtpNalUnitBatch result;
    result.timestamp = packet.timestamp;
    result.marker = packet.marker;
    const std::uint8_t type = packet.payload[0] & 0x1f;
    if (type >= 1 && type <= 23) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("H264 single NAL interrupted FU-A"));
        }
        if (!validH264NalHeader(packet.payload[0])) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("H264 RTP single NAL header is invalid"));
        }
        result.nalUnits.push_back(MediaRtpNalUnit::borrowed(packet.payload));
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (type == 24) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("H264 STAP-A interrupted FU-A"));
        }
        if (packet.payload.size() == 1) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("H264 STAP-A contains no NAL units"));
        }
        std::size_t offset = 1;
        while (offset < packet.payload.size()) {
            if (packet.payload.size() - offset < 2) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("H264 STAP-A truncated NAL length"));
            }
            const std::size_t length =
                (static_cast<std::size_t>(packet.payload[offset]) << 8) |
                packet.payload[offset + 1];
            offset += 2;
            if (length == 0 || length > packet.payload.size() - offset) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("H264 STAP-A invalid NAL length"));
            }
            if (!validH264NalHeader(packet.payload[offset])) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("H264 STAP-A contains invalid NAL header"));
            }
            result.nalUnits.push_back(MediaRtpNalUnit::borrowed(
                std::span<const std::uint8_t>(packet.payload).subspan(offset, length)));
            offset += length;
        }
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (type != 28) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            ::media::ErrorInfo::unsupported(
                "H264 RTP packetization type is not planned (STAP-B/MTAP/FU-B unsupported)"));
    }
    if (packet.payload.size() < 3) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 FU-A is truncated"));
    }
    const std::uint8_t header = packet.payload[1];
    const bool start = (header & 0x80) != 0;
    const bool end = (header & 0x40) != 0;
    const std::uint8_t fragmentedType = header & 0x1f;
    if ((header & 0x20) != 0 || (start && end) || fragmentedType == 0 ||
        fragmentedType > 23) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 FU-A flags are invalid"));
    }
    const std::uint8_t nalHeader = static_cast<std::uint8_t>(
        (packet.payload[0] & 0xe0) | fragmentedType);
    if (start) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("H264 FU-A duplicate start"));
        }
        m_fragmentHeader = nalHeader;
        m_fragmentTimestamp = packet.timestamp;
        m_fragment.clear();
        m_fragment.push_back(nalHeader);
    } else if (!m_fragmentHeader) {
        result.discardedFragment = true;
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (!m_fragmentTimestamp || *m_fragmentTimestamp != packet.timestamp ||
        *m_fragmentHeader != nalHeader) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 FU-A continuation changed timestamp, NRI, or NAL type"));
    }
    m_fragment.insert(m_fragment.end(), packet.payload.begin() + 2,
                      packet.payload.end());
    if (packet.marker && !end) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("H264 marker closed an incomplete FU-A"));
    }
    if (end) {
        result.nalUnits.push_back(MediaRtpNalUnit::owned(std::move(m_fragment)));
        m_fragment.clear();
        m_fragmentHeader.reset();
        m_fragmentTimestamp.reset();
    }
    return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
}

void MediaH264RtpNalUnitParser::discontinuity() noexcept
{
    m_fragment.clear();
    m_fragmentHeader.reset();
    m_fragmentTimestamp.reset();
}

MediaHevcRtpNalUnitParser::MediaHevcRtpNalUnitParser(
    std::uint8_t payloadType) noexcept
    : m_payloadType(payloadType)
{
}

::media::Result<MediaRtpNalUnitBatch> MediaHevcRtpNalUnitParser::push(
    const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_payloadType || packet.payload.size() < 2) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC RTP payload identity is invalid or truncated"));
    }
    if (!validHevcNalHeader(packet.payload.data())) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC RTP payload NAL header is invalid"));
    }

    MediaRtpNalUnitBatch result;
    result.timestamp = packet.timestamp;
    result.marker = packet.marker;
    const std::uint8_t type = static_cast<std::uint8_t>(
        (packet.payload[0] >> 1) & 0x3f);
    if (type <= 47) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("HEVC single NAL interrupted FU"));
        }
        result.nalUnits.push_back(MediaRtpNalUnit::borrowed(packet.payload));
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (type == 48) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("HEVC AP interrupted FU"));
        }
        if (packet.payload.size() == 2) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("HEVC AP contains no NAL units"));
        }
        std::size_t offset = 2;
        while (offset < packet.payload.size()) {
            if (packet.payload.size() - offset < 2) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("HEVC AP truncated NAL length"));
            }
            const std::size_t length =
                (static_cast<std::size_t>(packet.payload[offset]) << 8) |
                packet.payload[offset + 1];
            offset += 2;
            if (length < 2 || length > packet.payload.size() - offset) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("HEVC AP invalid NAL length"));
            }
            const auto* data = packet.payload.data() + offset;
            const std::uint8_t aggregatedType = static_cast<std::uint8_t>(
                (data[0] >> 1) & 0x3f);
            if (!validHevcNalHeader(data) || aggregatedType > 47) {
                return ::media::Result<MediaRtpNalUnitBatch>::failure(
                    invalidPayload("HEVC AP contains invalid NAL header"));
            }
            result.nalUnits.push_back(MediaRtpNalUnit::borrowed(
                std::span<const std::uint8_t>(packet.payload).subspan(offset, length)));
            offset += length;
        }
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (type != 49) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            ::media::ErrorInfo::unsupported(
                "HEVC RTP PACI or unsupported packetization mode"));
    }
    if (packet.payload.size() < 4) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC FU is truncated"));
    }
    const std::uint8_t fuHeader = packet.payload[2];
    const bool start = (fuHeader & 0x80) != 0;
    const bool end = (fuHeader & 0x40) != 0;
    const std::uint8_t fragmentedType = fuHeader & 0x3f;
    if ((start && end) || fragmentedType > 47) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC FU flags are invalid"));
    }
    const std::uint8_t reconstructedFirst = static_cast<std::uint8_t>(
        (packet.payload[0] & 0x81) | (fragmentedType << 1));
    const std::uint16_t reconstructedHeader = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(reconstructedFirst) << 8) |
        packet.payload[1]);
    if (start) {
        if (m_fragmentHeader) {
            return ::media::Result<MediaRtpNalUnitBatch>::failure(
                invalidPayload("HEVC FU duplicate start"));
        }
        m_fragmentHeader = reconstructedHeader;
        m_fragmentTimestamp = packet.timestamp;
        m_fragment.clear();
        m_fragment.push_back(reconstructedFirst);
        m_fragment.push_back(packet.payload[1]);
    } else if (!m_fragmentHeader) {
        result.discardedFragment = true;
        return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
    }
    if (!m_fragmentTimestamp || *m_fragmentTimestamp != packet.timestamp ||
        *m_fragmentHeader != reconstructedHeader) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC FU continuation changed timestamp or NAL identity"));
    }
    m_fragment.insert(m_fragment.end(), packet.payload.begin() + 3,
                      packet.payload.end());
    if (packet.marker && !end) {
        return ::media::Result<MediaRtpNalUnitBatch>::failure(
            invalidPayload("HEVC marker closed an incomplete FU"));
    }
    if (end) {
        result.nalUnits.push_back(MediaRtpNalUnit::owned(std::move(m_fragment)));
        m_fragment.clear();
        m_fragmentHeader.reset();
        m_fragmentTimestamp.reset();
    }
    return ::media::Result<MediaRtpNalUnitBatch>::success(std::move(result));
}

void MediaHevcRtpNalUnitParser::discontinuity() noexcept
{
    m_fragment.clear();
    m_fragmentHeader.reset();
    m_fragmentTimestamp.reset();
}

} // namespace media::ffmpeg::graph
