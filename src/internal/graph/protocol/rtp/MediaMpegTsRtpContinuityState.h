#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace media::ffmpeg::graph {

struct MediaMpegTsRtpCounterSnapshot final {
    std::uint64_t packetCount;
    std::uint64_t octetCount;
};

class MediaMpegTsRtpContinuityState;

class MediaMpegTsRtpPacketCommitReservation final {
public:
    MediaMpegTsRtpPacketCommitReservation(
        MediaMpegTsRtpPacketCommitReservation&&) noexcept = default;
    MediaMpegTsRtpPacketCommitReservation& operator=(
        MediaMpegTsRtpPacketCommitReservation&&) noexcept = default;
    MediaMpegTsRtpPacketCommitReservation(
        const MediaMpegTsRtpPacketCommitReservation&) = delete;
    MediaMpegTsRtpPacketCommitReservation& operator=(
        const MediaMpegTsRtpPacketCommitReservation&) = delete;

    std::uint64_t packetCount() const noexcept;
    std::uint64_t octetCount() const noexcept;
    std::uint16_t sequenceNumber() const noexcept;
    ::media::Status commit() noexcept;

private:
    friend class MediaMpegTsRtpContinuityState;

    MediaMpegTsRtpPacketCommitReservation(
        MediaMpegTsRtpContinuityState& state,
        std::size_t payloadOctets,
        std::unique_lock<std::mutex> lock) noexcept;

    MediaMpegTsRtpContinuityState* m_state;
    std::size_t m_payloadOctets;
    std::unique_lock<std::mutex> m_lock;
    bool m_committed = false;
};

class MediaMpegTsRtpContinuityState final {
public:
    static ::media::Result<std::shared_ptr<MediaMpegTsRtpContinuityState>>
    create(std::uint16_t initialSequenceNumber);

    ::media::Result<MediaMpegTsRtpPacketCommitReservation> reservePacket(
        std::size_t payloadOctets);
    MediaMpegTsRtpCounterSnapshot counterSnapshot() const noexcept;

private:
    friend class MediaMpegTsRtpPacketCommitReservation;

    explicit MediaMpegTsRtpContinuityState(
        std::uint16_t initialSequenceNumber) noexcept;

    mutable std::mutex m_counterMutex;
    std::uint16_t m_nextSequenceNumber;
    std::uint64_t m_packetCount = 0;
    std::uint64_t m_octetCount = 0;
};

} // namespace media::ffmpeg::graph
