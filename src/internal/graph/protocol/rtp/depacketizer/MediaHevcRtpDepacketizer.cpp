#include "internal/graph/protocol/rtp/depacketizer/MediaHevcRtpDepacketizer.h"

namespace media::ffmpeg::graph {
namespace {

constexpr uint8_t StartCode[] = {0, 0, 0, 1};
void appendNal(std::vector<uint8_t>& output, const uint8_t* data, std::size_t size)
{
    output.insert(output.end(), std::begin(StartCode), std::end(StartCode));
    output.insert(output.end(), data, data + size);
}
bool keyType(uint8_t type) noexcept { return type >= 16 && type <= 21; }
bool validNalHeader(const uint8_t* data) noexcept
{
    return (data[0] & 0x80) == 0 && (data[1] & 0x07) != 0;
}

} // namespace

MediaHevcRtpDepacketizer::MediaHevcRtpDepacketizer(MediaRtpDepacketizerConfig config)
    : m_config(std::move(config))
{
}

::media::Result<MediaRtpDepacketizerResult> MediaHevcRtpDepacketizer::push(const MediaRtpPacket& packet)
{
    auto result = pushValidated(packet);
    if (!result) discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    return result;
}

::media::Result<MediaRtpDepacketizerResult> MediaHevcRtpDepacketizer::pushValidated(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_config.payloadType || packet.payload.size() < 2) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("HEVC RTP payload identity is invalid or truncated"));
    if (!validNalHeader(packet.payload.data())) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("HEVC RTP payload NAL header is invalid"));
    if (m_timestamp && *m_timestamp != packet.timestamp && !m_accessUnit.empty()) discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    if (!m_timestamp) m_timestamp = packet.timestamp;
    const uint8_t type = static_cast<uint8_t>((packet.payload[0] >> 1) & 0x3f);
    if (type <= 47) {
        if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC single NAL interrupted FU"));
        appendNal(m_accessUnit, packet.payload.data(), packet.payload.size());
        m_keyFrame = m_keyFrame || keyType(type);
    } else if (type == 48) {
        if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC AP interrupted FU"));
        if (packet.payload.size() == 2) {
            discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("HEVC AP contains no NAL units"));
        }
        std::size_t offset = 2;
        while (offset < packet.payload.size()) {
            if (packet.payload.size() - offset < 2) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("HEVC AP truncated NAL length"));
            const std::size_t length = (static_cast<std::size_t>(packet.payload[offset]) << 8) | packet.payload[offset + 1];
            offset += 2;
            if (length < 2 || length > packet.payload.size() - offset) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("HEVC AP invalid NAL length"));
            const uint8_t aggregatedType = static_cast<uint8_t>((packet.payload[offset] >> 1) & 0x3f);
            if (!validNalHeader(packet.payload.data() + offset) || aggregatedType > 47) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("HEVC AP contains invalid NAL header"));
            m_keyFrame = m_keyFrame || keyType(aggregatedType);
            appendNal(m_accessUnit, packet.payload.data() + offset, length);
            offset += length;
        }
    } else if (type == 49) {
        if (packet.payload.size() < 4) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC FU is truncated"));
        const uint8_t fuHeader = packet.payload[2];
        const bool start = (fuHeader & 0x80) != 0;
        const bool end = (fuHeader & 0x40) != 0;
        const uint8_t fragmentedType = fuHeader & 0x3f;
        if ((start && end) || (fuHeader & 0x20) != 0 || fragmentedType > 47) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC FU flags are invalid"));
        if (start) {
            if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("HEVC FU duplicate start"));
            m_fragmentOpen = true;
            const uint8_t header[2] = {
                static_cast<uint8_t>((packet.payload[0] & 0x81) | ((fuHeader & 0x3f) << 1)),
                packet.payload[1]
            };
            m_fragmentNalHeader = static_cast<uint16_t>(
                (static_cast<uint16_t>(header[0]) << 8) | header[1]);
            appendNal(m_accessUnit, header, 2);
            m_keyFrame = m_keyFrame || keyType(fragmentedType);
        } else if (!m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC FU continuation has no valid start"));
        const uint8_t reconstructedFirst = static_cast<uint8_t>(
            (packet.payload[0] & 0x81) | (fragmentedType << 1));
        const uint16_t reconstructedHeader = static_cast<uint16_t>(
            (static_cast<uint16_t>(reconstructedFirst) << 8) | packet.payload[1]);
        if (!m_fragmentNalHeader || *m_fragmentNalHeader != reconstructedHeader) {
            discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "HEVC FU continuation changed NAL type, layer ID, or temporal ID"));
        }
        m_accessUnit.insert(m_accessUnit.end(), packet.payload.begin() + 3, packet.payload.end());
        if (end) { m_fragmentOpen = false; m_fragmentNalHeader.reset(); }
    } else {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::unsupported("HEVC RTP PACI or unsupported packetization mode"));
    }
    if (!packet.marker) return ::media::Result<MediaRtpDepacketizerResult>::success({});
    return finish(packet);
}

::media::Result<MediaRtpDepacketizerResult> MediaHevcRtpDepacketizer::finish(const MediaRtpPacket& packet)
{
    if (m_fragmentOpen || m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("HEVC marker closed an incomplete access unit"));
    }
    auto unit = makeRtpAccessUnit(std::move(m_accessUnit), packet.timestamp, m_config.clockRate, 0, m_keyFrame);
    m_accessUnit.clear(); m_timestamp.reset(); m_keyFrame = false;
    if (!unit) return ::media::Result<MediaRtpDepacketizerResult>::failure(unit.error());
    MediaRtpDepacketizerResult result; result.accessUnits.push_back(std::move(unit).value());
    return ::media::Result<MediaRtpDepacketizerResult>::success(std::move(result));
}

void MediaHevcRtpDepacketizer::discontinuity(MediaRtpDiscontinuityReason) noexcept
{
    m_accessUnit.clear(); m_timestamp.reset(); m_fragmentNalHeader.reset(); m_fragmentOpen = false; m_keyFrame = false;
}

} // namespace media::ffmpeg::graph
