#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"

#include <limits>

namespace media::ffmpeg::graph {

MediaAacRtpDepacketizer::MediaAacRtpDepacketizer(MediaRtpDepacketizerConfig config, MediaLineageAllocationScope allocation)
    : MediaRtpDepacketizer(std::move(allocation)), m_config(std::move(config))
{
}

MediaAacRtpPacketPreparation MediaAacRtpDepacketizer::preparePacket(
    const MediaRtpPacket& packet, std::optional<std::uint64_t> maximumAccessUnits) const
{
    const auto reject = [](const char* message, bool discardFragment) -> MediaAacRtpPacketPreparation {
        return MediaRtpPacketRejection{::media::ErrorInfo::invalidArgument(message), discardFragment};
    };
    if (packet.payloadType != m_config.payloadType)
        return reject("AAC MPEG4-GENERIC RTP payload type changed", false);
    auto headerPlan = MediaAacRtpAuHeaderPlanner::plan(packet.payload, allocation());
    if (!headerPlan) return MediaRtpPacketRejection{headerPlan.error(), false};
    const auto& headers = headerPlan.value().accessUnits;
    if (maximumAccessUnits && headers.size() > *maximumAccessUnits)
        return reject("AAC RTP push exceeds its prepared completion bound", false);
    const auto payloadBytes = headerPlan.value().payloadBytes;
    auto kind = MediaAacRtpPacketKind::Complete;
    if (m_fragmentedAccessUnit) {
        const auto& fragment = *m_fragmentedAccessUnit;
        if (headers.size() != 1 || headers.front().size != fragment.expectedSize ||
            headers.front().index != fragment.index || packet.timestamp != fragment.timestamp ||
            packet.sequenceNumber != static_cast<std::uint16_t>(fragment.lastSequenceNumber + 1) ||
            fragment.bytes.size() > fragment.expectedSize ||
            payloadBytes > fragment.expectedSize - fragment.bytes.size())
            return reject("AAC MPEG4-GENERIC fragmented AU continuity is invalid", true);
        const bool complete = payloadBytes == fragment.expectedSize - fragment.bytes.size();
        if (!complete && packet.marker)
            return reject("AAC MPEG4-GENERIC fragmented AU ended before its planned size", true);
        if (complete && !packet.marker)
            return reject("AAC MPEG4-GENERIC completed fragmented AU requires marker", true);
        kind = complete ? MediaAacRtpPacketKind::FragmentCompletion : MediaAacRtpPacketKind::FragmentContinuation;
    } else if (headerPlan.value().totalAccessUnitBytes > payloadBytes) {
        if (headers.size() != 1 || packet.marker || payloadBytes == 0)
            return reject("AAC MPEG4-GENERIC fragmented AU start is invalid", false);
        kind = MediaAacRtpPacketKind::FragmentStart;
    } else if (headerPlan.value().totalAccessUnitBytes != payloadBytes) {
        return reject("AAC MPEG4-GENERIC aggregate AU sizes do not partition payload", false);
    } else {
        const auto firstIndex = headers.front().index;
        for (const auto& header : headers) {
            const auto relativeIndex = header.index - firstIndex;
            const auto duration = static_cast<std::uint64_t>(m_config.accessUnitDurationRtpTicks);
            if (relativeIndex != 0 && duration > (std::numeric_limits<std::uint64_t>::max)() / relativeIndex)
                return reject("AAC MPEG4-GENERIC AU timestamp offset overflows", false);
        }
    }
    return MediaAacRtpPacketPlan{std::move(headerPlan).value(), kind};
}

::media::Result<MediaRtpDepacketizerResult> MediaAacRtpDepacketizer::push(const MediaRtpPacket& packet)
{
    using Result = ::media::Result<MediaRtpDepacketizerResult>;
    auto prepared = preparePacket(packet, std::nullopt);
    if (auto* rejected = std::get_if<MediaRtpPacketRejection>(&prepared)) {
        auto failure = commitRejection(*rejected);
        return Result::failure(failure.error());
    }
    const auto& plan = std::get<MediaAacRtpPacketPlan>(prepared);
    const auto& headers = plan.headers.accessUnits;
    const auto payloadOffset = plan.headers.payloadOffset;
    if (plan.kind == MediaAacRtpPacketKind::FragmentContinuation ||
        plan.kind == MediaAacRtpPacketKind::FragmentCompletion) {
        auto& fragment = *m_fragmentedAccessUnit;
        fragment.bytes.insert(fragment.bytes.end(), packet.payload.begin() + payloadOffset, packet.payload.end());
        fragment.lastSequenceNumber = packet.sequenceNumber;
        if (plan.kind == MediaAacRtpPacketKind::FragmentContinuation)
            return Result::success(MediaRtpDepacketizerResult{allocation()});
        auto unit = makeRtpAccessUnit(std::move(fragment.bytes), fragment.timestamp,
            m_config.clockRate, m_config.accessUnitDurationRtpTicks, true);
        m_fragmentedAccessUnit.reset();
        if (!unit) return Result::failure(unit.error());
        MediaRtpDepacketizerResult result{allocation()};
        result.accessUnits.push_back(std::move(unit).value());
        return Result::success(std::move(result));
    }
    if (plan.kind == MediaAacRtpPacketKind::FragmentStart) {
        m_fragmentedAccessUnit.emplace(FragmentedAccessUnit{
            headers.front().size, headers.front().index, packet.timestamp, packet.sequenceNumber,
            std::vector<std::uint8_t>(packet.payload.begin() + payloadOffset, packet.payload.end())});
        return Result::success(MediaRtpDepacketizerResult{allocation()});
    }
    MediaRtpDepacketizerResult result{allocation()};
    result.accessUnits.reserve(headers.size());
    std::size_t offset = payloadOffset;
    const auto firstIndex = headers.front().index;
    for (const auto& header : headers) {
        std::vector<std::uint8_t> bytes(packet.payload.begin() + offset, packet.payload.begin() + offset + header.size);
        const auto timestampOffset = (header.index - firstIndex) * static_cast<std::uint64_t>(m_config.accessUnitDurationRtpTicks);
        const auto timestamp = packet.timestamp + static_cast<std::uint32_t>(timestampOffset);
        auto unit = makeRtpAccessUnit(std::move(bytes), timestamp, m_config.clockRate,
            m_config.accessUnitDurationRtpTicks, true);
        if (!unit) return Result::failure(unit.error());
        result.accessUnits.push_back(std::move(unit).value());
        offset += header.size;
    }
    return Result::success(std::move(result));
}

::media::Status MediaAacRtpDepacketizer::commitRejection(const MediaRtpPacketRejection& rejection)
{
    if (rejection.discardFragment) m_fragmentedAccessUnit.reset();
    return ::media::Status::failure(rejection.error);
}

void MediaAacRtpDepacketizer::discontinuity(MediaRtpDiscontinuityReason) noexcept
{
    m_fragmentedAccessUnit.reset();
}

} // namespace media::ffmpeg::graph
