#include "internal/graph/protocol/rtp/depacketizer/MediaH264RtpDepacketizer.h"

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint8_t StartCode[] = {0, 0, 0, 1};

void appendNal(std::vector<std::uint8_t>& output,
               std::span<const std::uint8_t> nal)
{
    output.insert(output.end(), std::begin(StartCode), std::end(StartCode));
    output.insert(output.end(), nal.begin(), nal.end());
}

} // namespace

MediaH264RtpDepacketizer::MediaH264RtpDepacketizer(
    MediaRtpDepacketizerConfig config)
    : m_config(std::move(config)), m_nalParser(m_config.payloadType)
{
}

::media::Result<MediaRtpDepacketizerResult> MediaH264RtpDepacketizer::push(
    const MediaRtpPacket& packet)
{
    auto result = pushValidated(packet);
    if (!result) discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    return result;
}

::media::Result<MediaRtpDepacketizerResult>
MediaH264RtpDepacketizer::pushValidated(const MediaRtpPacket& packet)
{
    if (m_timestamp && *m_timestamp != packet.timestamp &&
        !m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    }
    if (!m_timestamp) m_timestamp = packet.timestamp;

    auto parsed = m_nalParser.push(packet);
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
        if (bytes.empty()) {
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "H264 RTP parser produced an empty NAL unit"));
        }
        m_keyFrame = m_keyFrame || (bytes[0] & 0x1f) == 5;
        appendNal(m_accessUnit, bytes);
    }
    if (!packet.marker) {
        return ::media::Result<MediaRtpDepacketizerResult>::success({});
    }
    return finish(packet);
}

::media::Result<MediaRtpDepacketizerResult> MediaH264RtpDepacketizer::finish(
    const MediaRtpPacket& packet)
{
    if (m_accessUnit.empty()) {
        discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "H264 marker closed an incomplete access unit"));
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

void MediaH264RtpDepacketizer::discontinuity(
    MediaRtpDiscontinuityReason) noexcept
{
    m_nalParser.discontinuity();
    m_accessUnit.clear();
    m_timestamp.reset();
    m_keyFrame = false;
}

} // namespace media::ffmpeg::graph
