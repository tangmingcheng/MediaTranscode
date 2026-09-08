#include "internal/graph/protocol/rtp/depacketizer/MediaHevcRtpDepacketizer.h"

namespace media::ffmpeg::graph {
namespace {

bool keyType(std::uint8_t type) noexcept
{
    return type >= 16 && type <= 21;
}

} // namespace

MediaHevcRtpDepacketizer::MediaHevcRtpDepacketizer(
    MediaRtpDepacketizerConfig config, std::size_t maximumAccessUnitBytes)
    : m_config(std::move(config)), m_maximumAccessUnitBytes(maximumAccessUnitBytes),
      m_nalParser(m_config.payloadType)
{
}

::media::Result<MediaRtpDepacketizerResult> MediaHevcRtpDepacketizer::push(
    const MediaRtpPacket& packet)
{
    auto result = pushValidated(packet);
    if (!result) discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    return result;
}

::media::Result<MediaRtpDepacketizerResult>
MediaHevcRtpDepacketizer::pushValidated(const MediaRtpPacket& packet)
{
    if (m_timestamp && *m_timestamp != packet.timestamp &&
        !m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    }
    if (!m_continuity.accept(packet.timestamp)) {
        return ::media::Result<MediaRtpDepacketizerResult>::success({});
    }
    if (!m_timestamp) m_timestamp = packet.timestamp;
    auto capacity = rtpAccessUnitNalCapacity(
        m_accessUnit.size(), m_maximumAccessUnitBytes);
    if (!capacity) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(capacity.error());
    }
    auto parsed = m_nalParser.push(packet, capacity.value());
    if (!parsed) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            parsed.error());
    }
    if (parsed.value().discardedFragment) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        return ::media::Result<MediaRtpDepacketizerResult>::success({});
    }
    for (const auto& nal : parsed.value().nalUnits) {
        const auto bytes = nal.bytes();
        if (bytes.size() < 2) {
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "HEVC RTP parser produced a truncated NAL unit"));
        }
        const std::uint8_t type = static_cast<std::uint8_t>(
            (bytes[0] >> 1) & 0x3f);
        m_keyFrame = m_keyFrame || keyType(type);
        if (auto status = appendRtpAccessUnitNal(
                m_accessUnit, bytes, m_maximumAccessUnitBytes); !status) {
            return ::media::Result<MediaRtpDepacketizerResult>::failure(status.error());
        }
    }
    if (!packet.marker) {
        return ::media::Result<MediaRtpDepacketizerResult>::success({});
    }
    return finish(packet);
}

::media::Result<MediaRtpDepacketizerResult> MediaHevcRtpDepacketizer::finish(
    const MediaRtpPacket& packet)
{
    if (m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "HEVC marker closed an incomplete access unit"));
    }
    auto unit = makeRtpAccessUnit(std::move(m_accessUnit), packet.timestamp,
                                  m_config.clockRate, 0, m_keyFrame);
    m_accessUnit.clear();
    m_timestamp.reset();
    m_keyFrame = false;
    if (!unit) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            unit.error());
    }
    MediaRtpDepacketizerResult result;
    result.accessUnits.push_back(std::move(unit).value());
    return ::media::Result<MediaRtpDepacketizerResult>::success(
        std::move(result));
}

void MediaHevcRtpDepacketizer::discontinuity(
    MediaRtpDiscontinuityReason) noexcept
{
    m_continuity.markLost();
    m_nalParser.discontinuity();
    m_accessUnit.clear();
    m_timestamp.reset();
    m_keyFrame = false;
}

} // namespace media::ffmpeg::graph
