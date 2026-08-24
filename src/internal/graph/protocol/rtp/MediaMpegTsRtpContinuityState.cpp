#include "internal/graph/protocol/rtp/MediaMpegTsRtpContinuityState.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t MaximumCumulativeCounter =
    (std::numeric_limits<std::uint64_t>::max)();

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

std::uint16_t
MediaMpegTsRtpPacketCommitReservation::sequenceNumber() const noexcept
{
    return m_state->m_nextSequenceNumber;
}

::media::Status MediaMpegTsRtpPacketCommitReservation::commit() noexcept
{
    if (m_committed || !m_state || !m_lock.owns_lock()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "MP2T RTP packet reservation cannot be committed twice or after move"));
    }
    ++m_state->m_nextSequenceNumber;
    ++m_state->m_packetCount;
    m_state->m_octetCount += m_payloadOctets;
    m_committed = true;
    return ::media::Status::success();
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

::media::Result<MediaMpegTsRtpPacketCommitReservation>
MediaMpegTsRtpContinuityState::reservePacket(
    std::size_t payloadOctets)
{
    std::unique_lock lock(m_counterMutex);
    if (payloadOctets == 0 ||
        m_packetCount == MaximumCumulativeCounter ||
        payloadOctets > MaximumCumulativeCounter - m_octetCount) {
        return ::media::Result<
            MediaMpegTsRtpPacketCommitReservation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T RTP cumulative sender counter would overflow"));
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
