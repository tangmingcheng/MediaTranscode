#pragma once

#include "internal/graph/protocol/rtp/MediaRtpInputClockSession.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketRejection.h"
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

using MediaRtpPacketPreparation = std::variant<MediaRtpPacketAllocationAction, MediaRtpPacketRejection>;

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
    ::media::Result<MediaRtpPreparedClockObservation> prepareDiscontinuity(std::uint64_t generationBefore) const;
    ::media::Status commitDiscontinuity(MediaRtpDiscontinuityReason reason,
        MediaRtpPreparedClockObservation prepared);
    ::media::Result<MediaRtpPacketPreparation> preparePacket(
        const MediaRtpPacket& packet, std::size_t heldReservations) const;
    ::media::Status commitPacketRejection(const MediaRtpPacketRejection& rejection);
    ::media::Result<MediaRtpPacketCompletion> commitPacket(
        const MediaRtpPacket& packet, std::size_t admittedReservations);
    MediaRtpAccessUnitAdmission admitAccessUnit(bool keyFrame) noexcept;
    // Caller must clear its reservations whenever either reset is invoked.
    void releasePayloadState() noexcept;
    void resetAssembly() noexcept;
    void requireKeyframe() noexcept;
    // Prepare, output materialization and commit share the caller's source lock
    // (or sole Raw owner thread). Never retain this product across a wait;
    // tracker identity/revision rejects stale state, pending RTP is not copied.
    ::media::Result<MediaRtpPreparedClockObservation> prepareRtcp(
        std::span<const std::uint8_t> bytes, std::int64_t observedAtNs, std::int64_t evaluationAtNs,
        MediaRtpByeHandling byeHandling) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareEvidence(std::int64_t evaluationAtNs) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareClockTransition(std::int64_t evaluationAtNs) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareMemberRetirement(
        std::uint64_t expectedIngressGeneration, std::uint32_t expectedSsrc,
        MediaRtpSourceUnavailableReason reason, std::int64_t observedAtNs) const;
    ::media::Status commitClockObservation(MediaRtpPreparedClockObservation&& prepared);
    ::media::Result<int> receiveTimeoutMs(std::chrono::steady_clock::time_point reorderNow,
        std::int64_t clockNowNs, int plannedMaximumMs) const;
    std::optional<std::int64_t> nextClockDeadlineNs() const noexcept;
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
