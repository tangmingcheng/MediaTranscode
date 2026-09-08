#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"

#include <limits>

namespace media::ffmpeg::graph {

MediaAacRtpDepacketizer::MediaAacRtpDepacketizer(MediaRtpDepacketizerConfig config)
    : m_config(std::move(config))
{
}

::media::Result<MediaRtpDepacketizerResult> MediaAacRtpDepacketizer::push(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_config.payloadType) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC RTP payload type changed"));
    auto headerPlan = MediaAacRtpAuHeaderPlanner::plan(packet.payload);
    if (!headerPlan) return ::media::Result<MediaRtpDepacketizerResult>::failure(headerPlan.error());
    const auto& headers = headerPlan.value().accessUnits;
    const std::size_t payloadOffset = headerPlan.value().payloadOffset;
    const std::size_t payloadBytes = headerPlan.value().payloadBytes;

    if (m_fragmentedAccessUnit) {
        auto& fragmented = *m_fragmentedAccessUnit;
        const bool consecutive =
            packet.sequenceNumber ==
            static_cast<std::uint16_t>(
                fragmented.lastSequenceNumber + 1);
        if (headers.size() != 1 ||
            headers.front().size != fragmented.expectedSize ||
            headers.front().index != fragmented.index ||
            packet.timestamp != fragmented.timestamp ||
            !consecutive ||
            payloadBytes >
                fragmented.expectedSize - fragmented.bytes.size()) {
            m_fragmentedAccessUnit.reset();
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AAC MPEG4-GENERIC fragmented AU continuity is invalid"));
        }
        fragmented.bytes.insert(
            fragmented.bytes.end(),
            packet.payload.begin() + payloadOffset,
            packet.payload.end());
        fragmented.lastSequenceNumber = packet.sequenceNumber;
        if (fragmented.bytes.size() < fragmented.expectedSize) {
            if (packet.marker) {
                m_fragmentedAccessUnit.reset();
                return ::media::Result<
                    MediaRtpDepacketizerResult>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "AAC MPEG4-GENERIC fragmented AU ended before its planned size"));
            }
            return ::media::Result<MediaRtpDepacketizerResult>::success({});
        }
        if (!packet.marker) {
            m_fragmentedAccessUnit.reset();
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AAC MPEG4-GENERIC completed fragmented AU requires marker"));
        }
        auto unit = makeRtpAccessUnit(
            std::move(fragmented.bytes),
            fragmented.timestamp,
            m_config.clockRate,
            m_config.accessUnitDurationRtpTicks,
            true);
        m_fragmentedAccessUnit.reset();
        if (!unit) {
            return ::media::Result<
                MediaRtpDepacketizerResult>::failure(unit.error());
        }
        MediaRtpDepacketizerResult result;
        result.accessUnits.push_back(std::move(unit).value());
        return ::media::Result<MediaRtpDepacketizerResult>::success(
            std::move(result));
    }

    const std::size_t totalAuBytes = headerPlan.value().totalAccessUnitBytes;
    if (totalAuBytes > payloadBytes) {
        if (headers.size() != 1 || packet.marker ||
            payloadBytes == 0) {
            return ::media::Result<MediaRtpDepacketizerResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AAC MPEG4-GENERIC fragmented AU start is invalid"));
        }
        m_fragmentedAccessUnit.emplace(FragmentedAccessUnit{
            headers.front().size,
            headers.front().index,
            packet.timestamp,
            packet.sequenceNumber,
            std::vector<std::uint8_t>(
                packet.payload.begin() + payloadOffset,
                packet.payload.end())});
        return ::media::Result<MediaRtpDepacketizerResult>::success({});
    }
    if (totalAuBytes != payloadBytes) {
        return ::media::Result<MediaRtpDepacketizerResult>::failure(
            ::media::ErrorInfo::invalidArgument("AAC MPEG4-GENERIC aggregate AU sizes do not partition payload"));
    }

    MediaRtpDepacketizerResult result;
    result.accessUnits.reserve(headers.size());
    std::size_t offset = payloadOffset;
    const uint64_t firstIndex = headers.front().index;
    for (const MediaAacRtpAuHeader& header : headers) {
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
    m_fragmentedAccessUnit.reset();
}

} // namespace media::ffmpeg::graph
