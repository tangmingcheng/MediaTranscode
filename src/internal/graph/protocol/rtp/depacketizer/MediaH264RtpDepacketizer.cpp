#include "internal/graph/protocol/rtp/depacketizer/MediaH264RtpDepacketizer.h"

namespace media::ffmpeg::graph {
namespace {

constexpr uint8_t StartCode[] = {0, 0, 0, 1};

void appendNal(std::vector<uint8_t>& output, const uint8_t* data, std::size_t size)
{
    output.insert(output.end(), std::begin(StartCode), std::end(StartCode));
    output.insert(output.end(), data, data + size);
}

bool validNalHeader(uint8_t header) noexcept
{
    const uint8_t type = header & 0x1f;
    return (header & 0x80) == 0 && type >= 1 && type <= 23;
}

} // namespace

MediaH264RtpDepacketizer::MediaH264RtpDepacketizer(MediaRtpDepacketizerConfig config)
    : m_config(std::move(config))
{
}

::media::Result<MediaRtpDepacketizerResult> MediaH264RtpDepacketizer::push(const MediaRtpPacket& packet)
{
    auto result = pushValidated(packet);
    if (!result) discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    return result;
}

::media::Result<MediaRtpDepacketizerResult> MediaH264RtpDepacketizer::pushValidated(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_config.payloadType || packet.payload.empty()) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 RTP packet has invalid payload identity or empty payload"));
    }
    if ((packet.payload[0] & 0x80) != 0) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("H264 RTP NAL forbidden_zero_bit is set"));
    if (m_timestamp && *m_timestamp != packet.timestamp && !m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    }
    if (!m_timestamp) m_timestamp = packet.timestamp;
    const uint8_t type = packet.payload[0] & 0x1f;
    if (type >= 1 && type <= 23) {
        if (!validNalHeader(packet.payload[0])) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 RTP single NAL header is invalid"));
        if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 single NAL interrupted FU-A"));
        appendNal(m_accessUnit, packet.payload.data(), packet.payload.size());
        m_keyFrame = m_keyFrame || type == 5;
    } else if (type == 24) {
        if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 STAP-A interrupted FU-A"));
        if (packet.payload.size() == 1) {
            discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 STAP-A contains no NAL units"));
        }
        std::size_t offset = 1;
        while (offset < packet.payload.size()) {
            if (packet.payload.size() - offset < 2) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 STAP-A truncated NAL length"));
            const std::size_t length = (static_cast<std::size_t>(packet.payload[offset]) << 8) | packet.payload[offset + 1];
            offset += 2;
            if (length == 0 || length > packet.payload.size() - offset) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 STAP-A invalid NAL length"));
            if (!validNalHeader(packet.payload[offset])) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 STAP-A contains invalid NAL header"));
            m_keyFrame = m_keyFrame || (packet.payload[offset] & 0x1f) == 5;
            appendNal(m_accessUnit, packet.payload.data() + offset, length);
            offset += length;
        }
    } else if (type == 28) {
        if (packet.payload.size() < 3) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 FU-A is truncated"));
        const uint8_t header = packet.payload[1];
        const bool start = (header & 0x80) != 0;
        const bool end = (header & 0x40) != 0;
        const uint8_t fragmentedType = header & 0x1f;
        if ((header & 0x20) != 0 || (start && end) || fragmentedType == 0 || fragmentedType > 23) return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 FU-A flags are invalid"));
        if (start) {
            if (m_fragmentOpen) return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 FU-A duplicate start"));
            m_fragmentOpen = true;
            const uint8_t nalHeader = static_cast<uint8_t>((packet.payload[0] & 0xe0) | (header & 0x1f));
            m_fragmentNalHeader = nalHeader;
            appendNal(m_accessUnit, &nalHeader, 1);
            m_keyFrame = m_keyFrame || fragmentedType == 5;
        } else if (!m_fragmentOpen) {
            discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return ::media::Result<MediaRtpDepacketizerResult>::success({});
        }
        const uint8_t nalHeader = static_cast<uint8_t>((packet.payload[0] & 0xe0) | fragmentedType);
        if (!m_fragmentNalHeader || *m_fragmentNalHeader != nalHeader) {
            discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument("H264 FU-A continuation changed NRI or NAL type"));
        }
        m_accessUnit.insert(m_accessUnit.end(), packet.payload.begin() + 2, packet.payload.end());
        if (end) { m_fragmentOpen = false; m_fragmentNalHeader.reset(); }
    } else {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::unsupported("H264 RTP packetization type is not planned (STAP-B/MTAP/FU-B unsupported)"));
    }
    if (!packet.marker) return ::media::Result<MediaRtpDepacketizerResult>::success({});
    return finish(packet);
}

::media::Result<MediaRtpDepacketizerResult> MediaH264RtpDepacketizer::finish(const MediaRtpPacket& packet)
{
    if (m_fragmentOpen || m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("H264 marker closed an incomplete access unit"));
    }
    auto unit = makeRtpAccessUnit(std::move(m_accessUnit), packet.timestamp, m_config.clockRate, 0, m_keyFrame);
    m_accessUnit.clear(); m_timestamp.reset(); m_keyFrame = false;
    if (!unit) return ::media::Result<MediaRtpDepacketizerResult>::failure(unit.error());
    MediaRtpDepacketizerResult result; result.accessUnits.push_back(std::move(unit).value());
    return ::media::Result<MediaRtpDepacketizerResult>::success(std::move(result));
}

void MediaH264RtpDepacketizer::discontinuity(MediaRtpDiscontinuityReason) noexcept
{
    m_accessUnit.clear(); m_timestamp.reset(); m_fragmentNalHeader.reset(); m_fragmentOpen = false; m_keyFrame = false;
}

} // namespace media::ffmpeg::graph
