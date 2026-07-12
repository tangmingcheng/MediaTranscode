#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"

#include <algorithm>

namespace media::ffmpeg::graph {
namespace {

int16_t sequenceDistance(uint16_t value, uint16_t base) noexcept
{
    return static_cast<int16_t>(value - base);
}

} // namespace

MediaRtpReorderBuffer::MediaRtpReorderBuffer(MediaRtpReorderConfig config)
    : m_config(config)
{
}

::media::Result<MediaRtpReorderResult> MediaRtpReorderBuffer::push(
    MediaRtpPacket packet,
    std::chrono::steady_clock::time_point receivedAt)
{
    if (m_config.windowPackets == 0 || m_config.maximumDelay.count() <= 0) {
        return ::media::Result<MediaRtpReorderResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTP reorder policy must be explicit and positive"));
    }
    MediaRtpReorderResult result;
    if (packet.payloadType != m_config.payloadType) {
        result.discontinuities.push_back(MediaRtpDiscontinuity{
            MediaRtpDiscontinuityReason::PayloadTypeChanged, packet.sequenceNumber, packet.sequenceNumber});
        return ::media::Result<MediaRtpReorderResult>::success(std::move(result));
    }
    if (m_ssrc && *m_ssrc != packet.ssrc) {
        result.discontinuities.push_back(MediaRtpDiscontinuity{
            MediaRtpDiscontinuityReason::SsrcChanged, packet.sequenceNumber, packet.sequenceNumber});
        m_expected = packet.sequenceNumber;
        m_pending.clear();
    }
    m_ssrc = packet.ssrc;
    if (!m_expected) m_expected = packet.sequenceNumber;

    const int16_t distance = sequenceDistance(packet.sequenceNumber, *m_expected);
    if (distance < 0 || m_pending.contains(packet.sequenceNumber)) {
        result.duplicate = true;
        return ::media::Result<MediaRtpReorderResult>::success(std::move(result));
    }
    m_pending.emplace(packet.sequenceNumber, Entry{std::move(packet), receivedAt});
    drainContiguous(result);

    if (!m_pending.empty()) {
        const auto nearest = nearestPending();
        if (m_pending.size() > m_config.windowPackets) {
            result.discontinuities.push_back(MediaRtpDiscontinuity{
                MediaRtpDiscontinuityReason::SequenceGap, *m_expected, nearest->first});
            m_expected = nearest->first;
            drainContiguous(result);
        }
    }
    auto expired = expire(receivedAt);
    if (!expired) return expired;
    result.packets.insert(result.packets.end(),
                          std::make_move_iterator(expired.value().packets.begin()),
                          std::make_move_iterator(expired.value().packets.end()));
    result.discontinuities.insert(result.discontinuities.end(),
                                  expired.value().discontinuities.begin(),
                                  expired.value().discontinuities.end());
    return ::media::Result<MediaRtpReorderResult>::success(std::move(result));
}

std::map<uint16_t, MediaRtpReorderBuffer::Entry>::const_iterator
MediaRtpReorderBuffer::nearestPending() const noexcept
{
    return std::min_element(m_pending.cbegin(), m_pending.cend(), [this](const auto& lhs, const auto& rhs) {
        return sequenceDistance(lhs.first, *m_expected) < sequenceDistance(rhs.first, *m_expected);
    });
}

std::optional<std::chrono::steady_clock::time_point> MediaRtpReorderBuffer::nextDeadline() const noexcept
{
    if (!m_expected || m_pending.empty() || m_config.maximumDelay.count() <= 0) return std::nullopt;
    const auto oldest = std::min_element(m_pending.cbegin(), m_pending.cend(), [](const auto& lhs, const auto& rhs) {
        return lhs.second.receivedAt < rhs.second.receivedAt;
    });
    return oldest->second.receivedAt + m_config.maximumDelay;
}

::media::Result<MediaRtpReorderResult> MediaRtpReorderBuffer::expire(
    std::chrono::steady_clock::time_point now)
{
    if (m_config.windowPackets == 0 || m_config.maximumDelay.count() <= 0) {
        return ::media::Result<MediaRtpReorderResult>::failure(
            ::media::ErrorInfo::invalidArgument("RTP reorder policy must be explicit and positive"));
    }
    MediaRtpReorderResult result;
    const auto deadline = nextDeadline();
    if (!deadline || now < *deadline) {
        return ::media::Result<MediaRtpReorderResult>::success(std::move(result));
    }
    const auto nearest = nearestPending();
    result.discontinuities.push_back(MediaRtpDiscontinuity{
        MediaRtpDiscontinuityReason::SequenceGap, *m_expected, nearest->first});
    m_expected = nearest->first;
    drainContiguous(result);
    return ::media::Result<MediaRtpReorderResult>::success(std::move(result));
}

void MediaRtpReorderBuffer::drainContiguous(MediaRtpReorderResult& result)
{
    while (m_expected) {
        auto found = m_pending.find(*m_expected);
        if (found == m_pending.end()) return;
        result.packets.push_back(std::move(found->second.packet));
        m_pending.erase(found);
        *m_expected = static_cast<uint16_t>(*m_expected + 1);
    }
}

void MediaRtpReorderBuffer::reset() noexcept
{
    m_expected.reset();
    m_ssrc.reset();
    m_pending.clear();
}

} // namespace media::ffmpeg::graph
