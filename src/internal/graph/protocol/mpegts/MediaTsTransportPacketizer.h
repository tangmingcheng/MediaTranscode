#pragma once
#include "internal/graph/planner/realtime/MediaTsSeriesStorageGeometry.h"

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketReservationWork.h"
#include "internal/graph/protocol/mpegts/MediaTsPreparedWireWork.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/protocol/mpegts/MediaTsSeriesStoragePreparation.h"
#include "internal/graph/planner/realtime/MediaTsSeriesStoragePlan.h"

namespace media::ffmpeg::graph {

struct MediaTsPacketizerControlState;
struct MediaTsPacketCursorState;
struct MediaTsPacketCursorFactory;
struct MediaTsPacketCursorDeleter final {
    std::pmr::memory_resource* resource;
    void operator()(MediaTsPacketCursorState* state) const noexcept;
};

// Unique within the packetizer owner; not a wire submission sequence.
struct MediaTsPacketReservationIdentity final {
    std::uint64_t cursorIdentity;
    std::uint64_t cursorRevision;
    std::uint64_t publicationIdentity;
    bool operator==(const MediaTsPacketReservationIdentity&) const noexcept = default;
};

struct MediaTsAcceptedReservationObservation final {
    MediaTsPacketReservationIdentity identity;
    MediaTsPacketReservationWork protocol;
    std::optional<MediaTsPreparedWireWork> prepared;
    std::optional<MediaTsBoundWireWork> bound;
};

// Holds the packetizer lock until the caller samples its global wire owner.
// Afterwards the exclusive scratch borrow remains live through planning.
class MediaTsAcceptedWorkRead final {
public:
    MediaTsAcceptedWorkRead(MediaTsAcceptedWorkRead&&) noexcept = default;
    MediaTsAcceptedWorkRead& operator=(MediaTsAcceptedWorkRead&&) = delete;
    MediaTsAcceptedWorkRead(const MediaTsAcceptedWorkRead&) = delete;
    ~MediaTsAcceptedWorkRead();
    std::span<MediaTsAcceptedReservationObservation> unlockForPlanning();
private:
    friend class MediaTsTransportPacketizer;
    MediaTsAcceptedWorkRead(std::shared_ptr<MediaTsPacketizerControlState> owner,
        std::unique_lock<std::mutex> lock) noexcept;
    std::shared_ptr<MediaTsPacketizerControlState> m_owner;
    std::unique_lock<std::mutex> m_lock;
};

class MediaTsPacketCommitToken final {
public:
    MediaTsPacketCommitToken(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken& operator=(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken(MediaTsPacketCommitToken&& other) noexcept;
    MediaTsPacketCommitToken& operator=(MediaTsPacketCommitToken&& other) noexcept;
    std::optional<MediaTsPacketReservationIdentity> reservationIdentity() const noexcept;

private:
    friend class MediaTsPacketCursor;
    MediaTsPacketCommitToken(std::weak_ptr<MediaTsPacketizerControlState> owner,
                             std::uint64_t cursorIdentity,
                             std::uint64_t revision,
                             std::optional<std::uint64_t> publicationIdentity) noexcept;

    std::weak_ptr<MediaTsPacketizerControlState> m_owner;
    std::uint64_t m_cursorIdentity = 0;
    std::uint64_t m_revision = 0;
    std::optional<std::uint64_t> m_publicationIdentity;
    bool m_valid = false;
};

class MediaTsPreparedPacketBatch final {
public:
    MediaTsPreparedPacketBatch(const MediaTsPreparedPacketBatch&) = delete;
    MediaTsPreparedPacketBatch& operator=(const MediaTsPreparedPacketBatch&) = delete;
    MediaTsPreparedPacketBatch(MediaTsPreparedPacketBatch&&) noexcept = default;
    MediaTsPreparedPacketBatch& operator=(MediaTsPreparedPacketBatch&&) noexcept = default;

    std::span<const std::array<std::uint8_t, 188>> packets() const noexcept;
    MediaTsPacketCommitToken takeCommitToken() noexcept;

private:
    friend class MediaTsPacketCursor;
    using PacketStorage = MediaFixedCapacityStorage<std::array<std::uint8_t, 188>>;
    MediaTsPreparedPacketBatch(std::shared_ptr<const PacketStorage> storage,
                               std::size_t begin,
                               std::size_t count,
                               MediaTsPacketCommitToken commitToken) noexcept;

    std::shared_ptr<const PacketStorage> m_storage;
    std::size_t m_begin;
    std::size_t m_count;
    MediaTsPacketCommitToken m_commitToken;
};

class MediaTsPreparedPacketSeries final {
public:
    MediaTsPreparedPacketSeries(const MediaTsPreparedPacketSeries&) = delete;
    MediaTsPreparedPacketSeries& operator=(const MediaTsPreparedPacketSeries&) = delete;
    MediaTsPreparedPacketSeries(MediaTsPreparedPacketSeries&&) noexcept = default;
    MediaTsPreparedPacketSeries& operator=(MediaTsPreparedPacketSeries&&) noexcept = default;

    std::span<const std::array<std::uint8_t, 188>> packets() const noexcept;
    MediaTsPacketCommitToken takeCommitToken() noexcept;

private:
    friend class MediaTsPacketCursor;
    using PacketStorage = MediaFixedCapacityStorage<std::array<std::uint8_t, 188>>;
    MediaTsPreparedPacketSeries(
        std::shared_ptr<const PacketStorage> storage,
        std::size_t begin,
        std::size_t count,
        MediaTsPacketCommitToken commitToken) noexcept;

    std::shared_ptr<const PacketStorage> m_storage;
    std::size_t m_begin;
    std::size_t m_count;
    MediaTsPacketCommitToken m_commitToken;
};

class MediaTsPacketCursor final {
public:
    ~MediaTsPacketCursor();
    MediaTsPacketCursor(const MediaTsPacketCursor&) = delete;
    MediaTsPacketCursor& operator=(const MediaTsPacketCursor&) = delete;
    MediaTsPacketCursor(MediaTsPacketCursor&& other) noexcept;
    MediaTsPacketCursor& operator=(MediaTsPacketCursor&& other) noexcept;

    ::media::Result<MediaTsPreparedPacketBatch> prepare(std::size_t maximumPackets);
    ::media::Result<MediaTsPreparedPacketSeries> prepareSeries(const MediaTsPacketReservationWork& work);
    ::media::Status registerPreparedWire(const MediaTsPacketReservationIdentity& identity,
        const MediaTsPreparedWireWork& work);
    ::media::Status bindPreparedWire(const MediaTsPacketReservationIdentity& identity,
        const MediaTsBoundWireRange& range);
    // Release construction ownership before the first reservation so an
    // adaptation-only PCR may precede this prepared payload. No CC advances.
    ::media::Status deferFirstReservation();
    MediaTsSeriesStoragePreparation& seriesStorage() noexcept;
    const MediaTsSeriesStoragePlan& storagePlan() const noexcept;
    const std::shared_ptr<MediaNetworkMemoryResource>& storageResource() const noexcept;
    ::media::Status commit(MediaTsPacketCommitToken commitToken);
    void poison() noexcept;
    bool finished() const noexcept;
    std::size_t remainingPacketCount() const noexcept;
    // Immutable packet bytes remain valid while this cursor is retained.
    // Unlike prepareSeries, this observation does not reserve continuity.
    ::media::Result<std::span<const std::array<std::uint8_t, 188>>> remainingPackets() const;

private:
    friend class MediaTsTransportPacketizer;
    friend struct MediaTsPacketCursorFactory;
    explicit MediaTsPacketCursor(std::unique_ptr<MediaTsPacketCursorState, MediaTsPacketCursorDeleter> state) noexcept;
    void cancel() noexcept;

    std::unique_ptr<MediaTsPacketCursorState, MediaTsPacketCursorDeleter> m_state;
};

enum class MediaTsMaintenanceSeries : std::size_t { Pat, Pmt, Pcr, Count };
using MediaTsMaintenanceStorage = std::array<std::optional<MediaTsSeriesStoragePreparation>,
    static_cast<std::size_t>(MediaTsMaintenanceSeries::Count)>;

class MediaTsTransportPacketizer final {
public:
    static constexpr std::size_t maintenancePreparationSlots() noexcept
    { return std::tuple_size_v<MediaTsMaintenanceStorage>; }
    ::media::Result<MediaTsMaintenanceStorage> prepareMaintenance(std::span<const std::size_t> packetCounts);
    static ::media::Result<MediaTsPacketizerSeriesStorageGeometry> seriesStorageGeometry();
    static ::media::Result<MediaSharedAllocationGeometry> controlStorageGeometry();
    static MediaSharedAllocationGeometry continuityReservationGeometry() noexcept;
    static MediaSharedAllocationGeometry acceptedObservationGeometry() noexcept;
    ::media::Result<MediaTsAcceptedWorkRead> lockAcceptedWork();
    static std::size_t continuityLedgerCount() noexcept;
    static ::media::Result<MediaTsTransportPacketizer> create(
        const MediaTsMuxPlan& plan,
        bool startsWithDiscontinuity,
        const MediaTsSeriesStoragePlan& storagePlan,
        std::shared_ptr<MediaNetworkMemoryResource> resource);

    MediaTsTransportPacketizer(const MediaTsTransportPacketizer&) = delete;
    MediaTsTransportPacketizer& operator=(const MediaTsTransportPacketizer&) = delete;
    MediaTsTransportPacketizer(MediaTsTransportPacketizer&&) noexcept = default;
    MediaTsTransportPacketizer& operator=(MediaTsTransportPacketizer&&) noexcept = default;

    ::media::Result<MediaTsPacketCursor> beginPat(const MediaTsPatSection& section, MediaTsSeriesStoragePreparation storage);
    ::media::Result<MediaTsPacketCursor> beginPmt(const MediaTsPmtSection& section, MediaTsSeriesStoragePreparation storage);
    ::media::Result<MediaTsPacketCursor> beginPcrOnly(const MediaTsPcrClock& pcr, MediaTsSeriesStoragePreparation storage);
    ::media::Result<std::size_t> patPacketCount(
        const MediaTsPatSection& section) const;
    ::media::Result<std::size_t> pmtPacketCount(
        const MediaTsPmtSection& section) const;
    ::media::Result<std::size_t> pcrOnlyPacketCount(
        const MediaTsPcrClock& pcr) const;
    ::media::Result<MediaTsPacketCursor> beginPes(
        MediaScheduledStream stream,
        const MediaTsPesHeader& header,
        std::span<const std::uint8_t> payload,
        bool randomAccess);

private:
    explicit MediaTsTransportPacketizer(
        std::shared_ptr<MediaTsPacketizerControlState> state) noexcept;

    std::shared_ptr<MediaTsPacketizerControlState> m_state;
};

} // namespace media::ffmpeg::graph
