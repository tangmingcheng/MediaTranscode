#pragma once

#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"
#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaTsPrefixPublicationPlanner.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaMpegTsProtocolSeriesState;
class MediaWireGlobalSequenceState;
class MediaWireGlobalSequenceReservation;
class MediaNodeWakeup;
struct MediaWireDatagramBatchInput;
struct MediaWireGlobalSequenceReservationEntry;

class MediaMpegTsProtocolDatagram final {
public:
    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    MediaWireMediaBoundary mediaBoundary() const noexcept { return m_mediaBoundary; }
    MediaRunningTime presentationOnMaster() const noexcept
    {
        return m_presentationOnMaster;
    }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_canonicalDeadline;
    }
    MediaRunningTime completionLimit() const noexcept { return m_completionLimit; }

private:
    friend class MediaMpegTsProtocolDatagramBatchBuffer;
    MediaMpegTsProtocolDatagram(
        std::span<const std::uint8_t> bytes,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline,
        MediaRunningTime completionLimit,
        MediaWireMediaBoundary mediaBoundary) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaRunningTime m_presentationOnMaster;
    MediaRunningTime m_canonicalRelease;
    MediaRunningTime m_canonicalDeadline;
    MediaRunningTime m_completionLimit;
    MediaWireMediaBoundary m_mediaBoundary;
};

class MediaMpegTsProtocolDatagramBatchBuffer final : public MediaBuffer {
public:
    class ConstructionKey final {
        friend class MediaMpegTsProtocolDatagramBatchBuffer;
        explicit ConstructionKey() = default;
    };
    static ::media::Result<MediaTsProtocolSeriesStorageGeometry> seriesStorageGeometry();
    MediaMpegTsProtocolDatagramBatchBuffer(ConstructionKey, std::uint64_t generation) noexcept;
    static ::media::Result<
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
    create(std::uint64_t generation,
           MediaTsPacketCursor cursor,
           std::uint16_t maximumPacketsPerDatagram,
           MediaRunningTime presentationOnMaster,
           MediaRunningTime canonicalRelease,
           MediaRunningTime canonicalDeadline,
           MediaWireMediaBoundary finalBoundary,
           std::size_t maximumReservationDatagrams,
           MediaRunningTime completionLimit,
           const MediaTsAcceptedWorkObservation& acceptedWork);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::MpegTsProtocolDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    std::uint64_t generation() const noexcept { return m_generation; }
    const std::optional<MediaTsPublicationObservation>& publicationObservation() const noexcept
    { return m_publicationObservation; }
    // Diagnostic observation set by the mux owner before publication.
    void observeMuxDequeued(MediaRunningTime time) noexcept { m_muxDequeuedAt = time; }
    // Sampled after construction/continuity commit, immediately before mux enqueue.
    void observeMuxPublicationReady(std::optional<MediaRunningTime> time) noexcept
    { if (m_publicationObservation) m_publicationObservation->muxPublicationReadyAt = time; }
    std::optional<MediaRunningTime> muxDequeuedAt() const noexcept { return m_muxDequeuedAt; }
    std::size_t materializedDatagrams() const noexcept { return m_materializedDatagrams - m_beginDatagram; }
    std::span<const MediaMpegTsProtocolDatagram> datagrams() const noexcept;
    // Mux-owner only, before publication. Transfers an exact prefix to a view;
    // caller supplies storage admitted for the real batch-owner geometry.
    ::media::Result<std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
    takePrefixView(std::size_t datagrams, MediaOwnedMemoryAllocation allocation);
    ::media::Status registerPreparedWire(std::span<const MediaWireDatagramBatchInput> inputs,
        std::span<const MediaWireGlobalSequenceReservationEntry> entries,
        std::size_t leadingControlDatagrams);
    void abandonPreparedWire(std::size_t protocolOffset) noexcept;
    ::media::Result<MediaProtocolDatagramCommitTransaction>
    takeCommitTransaction(std::size_t datagrams, MediaOwnedMemoryAllocation allocation,
        const std::shared_ptr<MediaWireGlobalSequenceState>& wireOwner,
        const MediaWireGlobalSequenceReservation& wireReservation,
        std::size_t leadingControlDatagrams);
    static MediaSharedAllocationGeometry prefixAllocationGeometry() noexcept;
    // Registers the producer before reporting WouldBlock. The caller must use
    // the worker's sequence-before-process wait protocol, not a polling loop.
    ::media::Status waitForCommit(const std::shared_ptr<MediaNodeWakeup>& producerWakeup,
        std::optional<std::int64_t>& protocolCommitCompletedSteadyNs) const;
    // Producer-only, after the previous reservation was transferred and
    // committed. Allocation must be admitted before continuity is advanced.
    ::media::Status reserveNextPrefix(std::size_t datagrams,
        const MediaTsPrefixPublicationWindow& window,
        MediaOwnedMemoryAllocation modelAllocation);

private:
    std::uint64_t m_generation;
    std::shared_ptr<MediaMpegTsProtocolSeriesState> m_series;
    std::size_t m_beginDatagram = 0;
    std::size_t m_endDatagram = 0;
    std::uint64_t m_payloadBytes = 0;
    std::size_t m_materializedDatagrams = 0;
    std::optional<MediaRunningTime> m_muxDequeuedAt;
    std::optional<MediaTsPublicationObservation> m_publicationObservation;
    struct PreparedRegistration final {
        MediaTsPacketReservationIdentity identity;
        std::size_t protocolBegin;
        std::size_t reservationBegin;
        std::size_t reservationEnd;
        std::size_t count;
        std::size_t leadingControlDatagrams;
    };
    std::optional<PreparedRegistration> m_preparedWire;
};

} // namespace media::ffmpeg::graph
