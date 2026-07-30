#include "internal/graph/protocol/rtp/MediaMpegTsRtpContinuityState.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t MaximumRtcpCounter =
    (std::numeric_limits<std::uint32_t>::max)();

} // namespace

MediaMpegTsRtpPacketCommitReservation::
    MediaMpegTsRtpPacketCommitReservation(
        MediaMpegTsRtpContinuityState& state,
        std::size_t payloadOctets,
        std::unique_lock<std::mutex> lock) noexcept
    : m_state(&state)
    , m_payloadOctets(payloadOctets)
    , m_lock(std::move(lock))
{
}

std::uint64_t
MediaMpegTsRtpPacketCommitReservation::packetCount() const noexcept
{
    return m_state->m_packetCount;
}

std::uint64_t
MediaMpegTsRtpPacketCommitReservation::octetCount() const noexcept
{
    return m_state->m_octetCount;
}

void MediaMpegTsRtpPacketCommitReservation::commit() noexcept
{
    if (m_committed) return;
    ++m_state->m_packetCount;
    m_state->m_octetCount += m_payloadOctets;
    m_committed = true;
}

MediaMpegTsRtpContinuityState::MediaMpegTsRtpContinuityState(
    std::uint16_t initialSequenceNumber) noexcept
    : m_nextSequenceNumber(initialSequenceNumber)
{
}

::media::Result<std::shared_ptr<MediaMpegTsRtpContinuityState>>
MediaMpegTsRtpContinuityState::create(
    std::uint16_t initialSequenceNumber)
{
    auto state = std::shared_ptr<MediaMpegTsRtpContinuityState>(
        new (std::nothrow)
            MediaMpegTsRtpContinuityState(initialSequenceNumber));
    if (!state) {
        return ::media::Result<
            std::shared_ptr<MediaMpegTsRtpContinuityState>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaMpegTsRtpContinuityState"));
    }
    return ::media::Result<
        std::shared_ptr<MediaMpegTsRtpContinuityState>>::success(
        std::move(state));
}

std::uint16_t
MediaMpegTsRtpContinuityState::takeSequenceNumber() noexcept
{
    return static_cast<std::uint16_t>(
        m_nextSequenceNumber.fetch_add(1, std::memory_order_relaxed));
}

::media::Result<MediaMpegTsRtpPacketCommitReservation>
MediaMpegTsRtpContinuityState::reservePacket(
    std::size_t payloadOctets)
{
    std::unique_lock lock(m_counterMutex);
    if (payloadOctets == 0 ||
        m_packetCount == MaximumRtcpCounter ||
        payloadOctets > MaximumRtcpCounter - m_octetCount) {
        return ::media::Result<
            MediaMpegTsRtpPacketCommitReservation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP sender counter increment would overflow"));
    }
    return ::media::Result<
        MediaMpegTsRtpPacketCommitReservation>::success(
        MediaMpegTsRtpPacketCommitReservation(
            *this, payloadOctets, std::move(lock)));
}

MediaMpegTsRtpCounterSnapshot
MediaMpegTsRtpContinuityState::counterSnapshot() const noexcept
{
    std::lock_guard lock(m_counterMutex);
    return MediaMpegTsRtpCounterSnapshot{
        m_packetCount, m_octetCount};
}

} // namespace media::ffmpeg::graph
