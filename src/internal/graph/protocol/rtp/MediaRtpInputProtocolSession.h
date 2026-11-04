#pragma once

#include "internal/graph/protocol/rtp/MediaRtpInputClockSession.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"
#include "internal/graph/runtime/storage/MediaLineageVector.h"
#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"

#include <deque>
#include <memory>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRtpPacketAllocationAction final {
    bool drop;
    bool releaseExisting;
    MediaLineageVector<std::uint64_t> reserveBytes;
};

struct MediaRtpPacketCompletion final {
    MediaRtpDepacketizerResult media;
    bool releaseReservations;
};

struct MediaRtpAccessUnitAdmission final {
    bool accepted;
    bool recoveredKeyframe;
};

// Single-owner ordered protocol state. Payload credits, graph buffers/events and
// transport belong to the caller. A pending front survives payload backpressure.
class MediaRtpInputProtocolSession final {
public:
    using PendingItem = std::variant<MediaRtpPacket,
        std::pair<MediaRtpDiscontinuity, std::uint64_t>>;
    static ::media::Result<MediaRtpInputProtocolSession> create(
        MediaRtpDepacketizerConfig config, MediaPreparedRtpAccessUnitEnvelope envelope,
        MediaRtpReorderConfig reorder, MediaRtpInputClockSessionConfig clock,
        std::unique_ptr<MediaRtpDepacketizer> depacketizer,
        MediaLineageAllocationScope allocation);
    MediaRtpInputProtocolSession(const MediaRtpInputProtocolSession&) = delete;
    MediaRtpInputProtocolSession& operator=(const MediaRtpInputProtocolSession&) = delete;
    MediaRtpInputProtocolSession(MediaRtpInputProtocolSession&&) = default;
    MediaRtpInputProtocolSession& operator=(MediaRtpInputProtocolSession&&) = default;

    ::media::Status observeRtp(MediaRtpPacket packet, std::int64_t observedAtNs);
    ::media::Status expire(std::chrono::steady_clock::time_point now);
    bool hasPending() const noexcept;
    const PendingItem& pendingFront() const;
    void consumeFront();
    void observeDiscontinuity(MediaRtpDiscontinuityReason reason,
        std::uint64_t generationBefore) noexcept;
    ::media::Result<MediaRtpPacketAllocationAction> preparePacket(
        const MediaRtpPacket& packet, std::size_t heldReservations) const;
    ::media::Result<MediaRtpPacketCompletion> commitPacket(
        const MediaRtpPacket& packet, std::size_t admittedReservations);
    MediaRtpAccessUnitAdmission admitAccessUnit(bool keyFrame) noexcept;
    // Caller must clear its reservations whenever either reset is invoked.
    void releasePayloadState() noexcept;
    void resetAssembly() noexcept;
    void requireKeyframe() noexcept;
    ::media::Result<MediaRtcpObservation> observeRtcp(
        std::span<const std::uint8_t> bytes, std::int64_t observedAtNs);
    ::media::Result<std::optional<MediaRtcpClockEvidence>> takeEvidenceUpdate(std::int64_t now);
    ::media::Result<std::optional<MediaRtpClockSessionTransition>> transition(std::int64_t now);
    ::media::Result<int> receiveTimeoutMs(std::chrono::steady_clock::time_point reorderNow,
        std::int64_t clockNowNs, int plannedMaximumMs) const;
    std::uint64_t generation() const noexcept;
    const MediaRtpInputClockSessionConfig& clockConfig() const noexcept;

private:
    MediaRtpInputProtocolSession(MediaRtpDepacketizerConfig config,
        MediaPreparedRtpAccessUnitEnvelope envelope, MediaRtpReorderConfig reorder,
        MediaRtpInputClockSession clock, std::unique_ptr<MediaRtpDepacketizer> depacketizer,
        MediaLineageAllocationScope allocation);
    ::media::Status enqueueReordered(MediaRtpReorderResult reordered, std::uint64_t generationBefore);
    MediaRtpDepacketizerConfig m_config;
    MediaPreparedRtpAccessUnitEnvelope m_envelope;
    MediaLineageUniquePtr<MediaRtpReorderBuffer> m_reorder;
    MediaRtpInputClockSession m_clock;
    std::unique_ptr<MediaRtpDepacketizer> m_depacketizer;
    MediaLineageAllocationScope m_allocation;
    MediaLineageUniquePtr<std::pmr::deque<PendingItem>> m_pending;
    std::size_t m_pendingCapacity;
    std::optional<std::uint32_t> m_reservedAccessUnitTimestamp;
    bool m_waitingForKeyFrame = false;
    bool m_waitingForClockEvidence = false;
};

} // namespace media::ffmpeg::graph
