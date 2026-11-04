#pragma once
#include "internal/graph/planner/realtime/MediaTsMuxQueueStoragePlan.h"
#include "internal/graph/planner/realtime/MediaTsSeriesStoragePlan.h"

#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/runtime/network/MediaDatagramSubmittedService.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include "internal/graph/runtime/resource/MediaRtpProtocolStorageRegistry.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaDatagramCommitStoragePlan.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaMasterClock;

class MediaNodeWakeup;

// The timestamp is the sender's physical-submit completion sample passed to
// commit(), not the time the ledger lock is released or waiters are notified.
struct MediaWireCompletedReservationObservation final {
    std::uint64_t identity;
    std::uint64_t firstSequence;
    std::uint64_t lastSequence;
    MediaRunningTime submittedAt;
};

struct MediaWireGlobalSequenceSnapshot final {
    std::uint64_t generation;
    std::uint64_t nextGlobalSequence;
    bool reservationActive;
    bool poisoned;
    std::uint64_t cancelledDatagrams;
    std::uint64_t cancelledWireBytes;
    std::uint64_t currentDatagrams;
    std::uint64_t currentWireBytes;
    std::uint64_t highWaterDatagrams;
    std::uint64_t highWaterWireBytes;
    std::uint64_t maximumResidenceNanoseconds;
    std::optional<std::uint64_t> lastMaterializedSequence;
    std::optional<std::uint64_t> lastScheduledSequence;
    std::optional<std::uint64_t> lastSubmittedSequence;
    std::optional<std::uint64_t> lastCommittedSequence;
    std::optional<MediaWireCompletedReservationObservation> lastCompletedReservation;
    std::optional<MediaDatagramSubmittedService> lastSubmittedService;
};

struct MediaWirePacingQueueSnapshot final {
    std::uint64_t wireBytes;
    MediaRunningTime averageResidence;
    MediaRunningTime sampledAt;
};

struct MediaWireGlobalSequenceReservationEntry final {
    std::uint64_t endpointId;
    std::uint64_t payloadBytes;
    MediaRunningTime materializedAt;
};

struct MediaWireGlobalSequencePreparedStorage final {
    MediaFixedCapacityStorage<std::uint64_t> wireBytes;
    MediaFixedCapacityStorage<MediaRunningTime> materializedAt;
};
class MediaWireGlobalSequenceState;

class MediaWireGlobalSequenceReservation final {
public:
    MediaWireGlobalSequenceReservation(
        MediaWireGlobalSequenceReservation&& other) noexcept;
    MediaWireGlobalSequenceReservation& operator=(
        MediaWireGlobalSequenceReservation&& other) noexcept;
    MediaWireGlobalSequenceReservation(
        const MediaWireGlobalSequenceReservation&) = delete;
    MediaWireGlobalSequenceReservation& operator=(
        const MediaWireGlobalSequenceReservation&) = delete;
    ~MediaWireGlobalSequenceReservation() noexcept;

    std::uint64_t generation() const noexcept;
    bool belongsTo(const MediaWireGlobalSequenceState& owner) const noexcept
    { return m_state.get() == &owner; }
    std::uint64_t identity() const noexcept { return m_reservationIdentity; }
    std::size_t size() const noexcept;
    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept;
    ::media::Status markScheduled(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;
    ::media::Status markSubmitted(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;
    ::media::Status canCommit(
        std::size_t begin, std::size_t count) const noexcept;
    ::media::Status commit(
        std::size_t begin,
        std::size_t count,
        const MediaDatagramSubmittedService& submitted) noexcept;

private:
    friend class MediaWireGlobalSequenceState;

    MediaWireGlobalSequenceReservation(
        std::shared_ptr<MediaWireGlobalSequenceState> state,
        std::uint64_t reservationIdentity,
        std::uint64_t firstSequence,
        MediaFixedCapacityStorage<std::uint64_t> wireBytes,
        MediaFixedCapacityStorage<MediaRunningTime> materializedAt) noexcept;
    void releaseCompleted() noexcept;
    void abandon() noexcept;

    std::shared_ptr<MediaWireGlobalSequenceState> m_state;
    std::uint64_t m_reservationIdentity = 0;
    std::uint64_t m_firstSequence = 0;
    MediaFixedCapacityStorage<std::uint64_t> m_wireBytes;
    MediaFixedCapacityStorage<MediaRunningTime> m_materializedAt;
    std::size_t m_committed = 0;
};

class MediaWireGlobalSequenceState final
    : public std::enable_shared_from_this<MediaWireGlobalSequenceState> {
public:
    static ::media::Result<std::shared_ptr<MediaWireGlobalSequenceState>>
    create(std::string sessionKey,
           std::string serviceScopeId,
           std::uint64_t generation,
           std::uint64_t firstGlobalSequence,
           std::size_t maximumOutstandingDatagrams,
           std::uint64_t maximumOutstandingWireBytes,
           bool commitBeforeNextReservation,
           std::shared_ptr<MediaNetworkMemoryResource> storageResource,
           std::unordered_map<std::uint64_t, std::uint64_t>
               endpointWireHeaderBytes,
           std::span<const MediaRtpProtocolStoragePlan> rtpStorageInstances,
           MediaDatagramCommitStoragePlan commitStorage,
            std::optional<MediaTsMuxQueueStoragePlan> muxQueueStorage,
    std::optional<MediaTsSeriesStoragePlan> seriesStorage);

    // Exact fixed backing used by this activation's reservation ledger.
    static ::media::Result<std::size_t> reservationStorageFootprint(
        std::uint64_t maximumOutstandingDatagrams);

    // Exclusive unpublished backing: failure preserves it unchanged; only a
    // successful reservation takes ownership. No allocation occurs on admission.
    ::media::Result<MediaWireGlobalSequenceReservation> reserve(
        std::span<const MediaWireGlobalSequenceReservationEntry> entries,
        MediaWireGlobalSequencePreparedStorage& storage);
    ::media::Result<MediaRtpProtocolStoragePermit> claimRtpProtocolStorage(
        std::uint64_t rtpEndpointId, std::uint64_t rtcpEndpointId,
        std::size_t capacity, std::shared_ptr<MediaNodeWakeup> wakeup);
    ::media::Status registerReservationWakeup(
        std::uint64_t endpointId,
        std::shared_ptr<MediaNodeWakeup> wakeup);
    ::media::Result<MediaWirePacingQueueSnapshot> pacingQueueSnapshot(
        const MediaMasterClock& clock) const;
    ::media::Status authorizeGenerationPurge(const MediaAvGenerationPurge& purge);
    MediaWireGlobalSequenceSnapshot snapshot() const noexcept;
    const std::string& sessionKey() const noexcept { return m_sessionKey; }
    const std::string& serviceScopeId() const noexcept
    {
        return m_serviceScopeId;
    }
    std::uint64_t generation() const noexcept { return m_generation; }
    const std::optional<MediaTsMuxQueueStoragePlan>& muxQueueStorage() const noexcept
    { return m_muxQueueStorage; }
    const std::optional<MediaTsSeriesStoragePlan>& seriesStorage() const noexcept
    { return m_seriesStorage; }
    const MediaDatagramCommitStoragePlan& commitStorage() const noexcept { return m_commitStorage; }
    const std::shared_ptr<MediaNetworkMemoryResource>& storageResource() const noexcept
    { return m_storageResource; }

private:
    friend class MediaWireGlobalSequenceReservation;

    MediaWireGlobalSequenceState(std::string sessionKey,
                                 std::string serviceScopeId,
                                 std::uint64_t generation,
                                 std::uint64_t firstGlobalSequence,
                                 std::size_t maximumOutstandingDatagrams,
                                 std::uint64_t maximumOutstandingWireBytes,
                                 bool commitBeforeNextReservation,
                                 std::shared_ptr<MediaNetworkMemoryResource> storageResource,
                                 std::unordered_map<std::uint64_t, std::uint64_t>
                                     endpointWireHeaderBytes,
                                 std::shared_ptr<MediaRtpProtocolStorageRegistry> protocolStorage,
                                 MediaDatagramCommitStoragePlan commitStorage,
            std::optional<MediaTsMuxQueueStoragePlan> muxQueueStorage,
    std::optional<MediaTsSeriesStoragePlan> seriesStorage);
    ::media::Status markStageRange(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now,
        std::optional<std::uint64_t>& lastStageSequence) noexcept;
    ::media::Status markStageRangeLocked(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now,
        std::optional<std::uint64_t>& lastStageSequence) noexcept;
    ::media::Status canCommitRangeLocked(
        const MediaWireGlobalSequenceReservation& reservation,
        std::size_t begin,
        std::size_t count) const noexcept;
    void observeResidence(
        MediaRunningTime materializedAt, MediaRunningTime now) noexcept;
    void notifyReservationWaiters() noexcept;

    struct ReservationRecord final {
        std::uint64_t identity;
        std::uint64_t firstSequence;
        std::size_t count;
        std::size_t committed;
    };

    const std::shared_ptr<MediaNetworkMemoryResource> m_storageResource;
    const MediaDatagramCommitStoragePlan m_commitStorage;
    const std::optional<MediaTsMuxQueueStoragePlan> m_muxQueueStorage;
    const std::optional<MediaTsSeriesStoragePlan> m_seriesStorage;
    const std::shared_ptr<MediaRtpProtocolStorageRegistry> m_protocolStorage;
    mutable std::mutex m_mutex;
    const std::string m_sessionKey;
    const std::string m_serviceScopeId;
    const std::uint64_t m_generation;
    const std::size_t m_maximumOutstandingDatagrams;
    const std::uint64_t m_maximumOutstandingWireBytes;
    const bool m_commitBeforeNextReservation;
    const std::unordered_map<std::uint64_t, std::uint64_t>
        m_endpointWireHeaderBytes;
    std::unordered_map<std::uint64_t, std::weak_ptr<MediaNodeWakeup>>
        m_reservationWakeups;
    std::uint64_t m_nextGlobalSequence;
    std::uint64_t m_projectedNextGlobalSequence;
    std::uint64_t m_nextReservationIdentity = 1;
    std::size_t m_outstandingDatagrams = 0;
    std::uint64_t m_outstandingWireBytes = 0;
    std::uint64_t m_highWaterDatagrams = 0;
    std::uint64_t m_highWaterWireBytes = 0;
    std::uint64_t m_maximumResidenceNanoseconds = 0;
    std::uint64_t m_queueResidenceSumNanoseconds = 0;
    std::optional<MediaRunningTime> m_queueResidenceUpdatedAt;
    std::optional<std::uint64_t> m_lastMaterializedSequence;
    std::optional<std::uint64_t> m_lastScheduledSequence;
    std::optional<std::uint64_t> m_lastSubmittedSequence;
    std::optional<std::uint64_t> m_lastCommittedSequence;
    std::optional<MediaWireCompletedReservationObservation> m_lastCompletedReservation;
    std::optional<MediaDatagramSubmittedService> m_lastSubmittedService;
    MediaFixedCapacityStorage<ReservationRecord> m_reservations;
    bool m_reservationBlocked = false;
    bool m_poisoned = false;
    std::optional<MediaAvGenerationPurge> m_authorizedPurge;
    std::uint64_t m_cancelledDatagrams = 0;
    std::uint64_t m_cancelledWireBytes = 0;
};

} // namespace media::ffmpeg::graph
