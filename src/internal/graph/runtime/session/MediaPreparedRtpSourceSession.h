#pragma once

#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpInputProtocolSession.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/runtime/resource/MediaPreparedRtpSourceBudgets.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/lifecycle/MediaInputActivity.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/model/MediaPreparedRtpStorageReservation.h"
#include "internal/graph/model/MediaPreparedRtpOutputPort.h"
#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"
#include "internal/graph/sync/MediaAvSourceDrain.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

enum class MediaPreparedRtpOutputClass { Packet, Control };

enum class MediaPreparedRtpCreditBlocker { PayloadBytes, PacketObjects, ControlObjects };
enum class MediaPreparedRtpIngressState { AcceptedReady, AcceptedWaitingCredit, NotAcceptedWaitingControl, NotAcceptedWaitingGeneration };
struct MediaPreparedRtpIngressProgress final {
    MediaPreparedRtpIngressState state;
    std::optional<MediaPreparedRtpCreditBlocker> blocker;
};


struct MediaPreparedRtpOutput final {
    MediaPreparedRtpOutputPort port;
    MediaBufferRef buffer;
    std::size_t admissionBytes;
};

struct MediaPreparedRtpPrefix final {
    explicit MediaPreparedRtpPrefix(std::shared_ptr<MediaPreparationMemoryResource> owner)
        : resource(std::move(owner)), packets(resource.get()) {}
    std::shared_ptr<MediaPreparationMemoryResource> resource;
    std::pmr::deque<MediaBufferRef> packets;
};

struct MediaPreparedRtpPacketClockHandoff final {
    MediaRtpPacketClockSession session;
    MediaLineageUniquePtr<MediaPreparedRtpPrefix> prefix;
};

struct MediaPreparedRtpAccessUnitSnapshot final {
    MediaPreparedRtpAccessUnitSnapshot(std::shared_ptr<MediaPreparationMemoryResource> resource,
        MediaLineageUniquePtr<std::pmr::vector<MediaBufferRef>> values, std::uint64_t frontier)
        : referenceResource(std::move(resource)), packetStorage(std::move(values)),
          packets(*packetStorage), revision(frontier) {}
    MediaPreparedRtpAccessUnitSnapshot(MediaPreparedRtpAccessUnitSnapshot&&) noexcept = default;
    MediaPreparedRtpAccessUnitSnapshot& operator=(MediaPreparedRtpAccessUnitSnapshot&&) = delete;
    MediaPreparedRtpAccessUnitSnapshot(const MediaPreparedRtpAccessUnitSnapshot&) = delete;
    MediaPreparedRtpAccessUnitSnapshot& operator=(const MediaPreparedRtpAccessUnitSnapshot&) = delete;
    // Actual allocation reservations retire with the container, before its resource owner.
    std::shared_ptr<MediaPreparationMemoryResource> referenceResource;
    // Stable vector ownership avoids allocating debug iterator proxies in a
    // noexcept vector move. Only this owner moves through Result temporaries.
    MediaLineageUniquePtr<std::pmr::vector<MediaBufferRef>> packetStorage;
    std::span<const MediaBufferRef> packets;
    std::uint64_t revision;
};

struct MediaPreparedRtpSourceStorageFacts final {
    std::size_t sourceBytes;
    std::size_t streamBytes;
    std::size_t streamAlignment;
    std::size_t outputEntryBytes;
    std::size_t eventBytes;
    std::size_t packetBytes;
    std::size_t referenceBytes;
};

// Preparation receivers serialize protocol state here; no socket wait, graph
// operation, or decoder call runs under this owner's mutex.
class MediaPreparedRtpSourceSession final {
    struct ConstructionKey final {};
    struct WorkingSet final {
        MediaPreparationStorageLease fixed;
        std::optional<MediaPreparationStorageLease> video;
        std::optional<MediaPreparationStorageLease> audio;
        std::optional<MediaPreparationStorageLease> videoCompressed;
        std::optional<MediaPreparationStorageLease> audioCompressed;
    };
public:
    // Public for allocate_shared; only create can supply the private key.
    MediaPreparedRtpSourceSession(ConstructionKey, MediaPreparedRtpSourceSessionPlan&& plan,
        MediaPreparedRtpSourceBudgets&& budgets,
        std::shared_ptr<MediaMasterClock>&& clock,
        std::shared_ptr<MediaPreparationMemoryResource> resource,
        MediaLineageUniquePtr<MediaRtpClockGroupSession>&& group, WorkingSet&& workingSet);
    static MediaPreparedRtpSourceStorageFacts storageFacts() noexcept;
    static ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>> create(
        MediaPreparedRtpSourceSessionPlan plan,
        MediaPreparedRtpSourceBudgets budgets,
        std::shared_ptr<MediaMasterClock> clock);
    ::media::Result<MediaPreparedRtpIngressProgress> consume(MediaStreamKind stream, MediaRtpUdpChannel channel,
        std::span<const std::uint8_t> bytes, std::int64_t observedAtNs,
        std::uint64_t ordinal);
    ::media::Status finishBootstrap(std::uint64_t videoFrontier,
        std::optional<std::uint64_t> audioFrontier);
    ::media::Result<MediaPreparedRtpIngressProgress> tick(MediaStreamKind stream, std::int64_t observedAtNs);
    ::media::Result<MediaPreparedRtpIngressProgress> resumeDrain(MediaStreamKind stream, std::int64_t now);
    // Called only by the RTP owner with no unprocessed batch or handoff datagram.
    ::media::Status observeRtpCaptureIdle(MediaStreamKind stream, std::int64_t observedAtNs);
    ::media::Result<MediaPreparedRtpAccessUnitSnapshot> probeSnapshot() const;
    ::media::Status waitForAccessUnits(std::uint64_t revision,
        std::chrono::steady_clock::time_point deadline, std::stop_token stop);
    static ::media::Status freezeBatch(
        std::span<const std::shared_ptr<MediaPreparedRtpSourceSession>> sources,
        const MediaPreparedRtpBatchAdmissionSolver& solver);
    bool isFrozen() const;
    ::media::Result<std::optional<MediaPreparedRtpOutput>> takeOutput(
        MediaStreamKind stream, MediaPreparedRtpOutputClass outputClass);
    ::media::Status activateControl(MediaStreamKind stream, std::shared_ptr<MediaNodeWakeup> wakeup);
    void deactivateControl(MediaStreamKind stream, const std::shared_ptr<MediaNodeWakeup>& wakeup) noexcept;
    ::media::Result<int> receiveTimeoutMs(MediaStreamKind stream, std::int64_t now,
        int plannedMaximumMs);
    ::media::Result<std::optional<std::chrono::nanoseconds>> creditWaitTimeout(
        MediaStreamKind stream, std::int64_t now);
    ::media::Status activateStream(MediaStreamKind stream,
        std::shared_ptr<MediaNodeWakeup> wakeup, std::shared_ptr<MediaInputActivity> activity);
    // Empty means the same purge still owns work; callers must not acknowledge it.
    ::media::Result<std::optional<std::uint64_t>> purgeMedia(MediaStreamKind stream, const MediaAvGenerationPurge& purge);
    ::media::Status bindRuntimeGroup(const std::shared_ptr<MediaAvSyncGroupRuntime>& group);
    ::media::Status observeRuntimeClockGroup(const MediaRtpClockGroupSnapshot& snapshot);
    ::media::Result<std::optional<MediaRtpClosedClockBoundary>> closedClockBoundary() const;
    ::media::Status observeConsumedControl(MediaStreamKind stream,
        std::uint64_t sequence, const std::optional<MediaRtpClockInvalidation>& invalidation);
    void fail(::media::ErrorInfo error);
    ::media::Status status() const;
    ::media::Status validatePreparationPhase() const;
    MediaPreparedRtpStorageReservation preparationStorage() const;
    ::media::Result<MediaPreparedRtpPacketClockHandoff> takePacketClock(
        const MediaRtpPacketClockSessionConfig& expected);
    ::media::Result<MediaRtpClockGroupSession> takeClockGroup(
        const MediaRtpClockGroupSessionConfig& expected);
    const MediaPreparedRtpSourceSessionPlan& plan() const noexcept { return m_plan; }
    // Called only after successful seal; the product is immutable thereafter.
    const MediaPreparedRtpStorageAdmission* sealedAdmission() const noexcept
    { return m_plan.sealedAdmission ? &*m_plan.sealedAdmission : nullptr; }
    const std::shared_ptr<MediaMasterClock>& clock() const noexcept { return m_clock; }
    const std::shared_ptr<MediaPreparationMemoryResource>& videoReferenceResource() const noexcept
    { return m_video->resource; }
private:
    struct ClosingMember final {
        bool endQueued = false;
        std::optional<std::uint64_t> frontier;
        std::optional<std::uint64_t> retirementSequence;
        std::optional<MediaRtpClockInvalidation> retirement;
        std::optional<std::uint64_t> purgeSequence;
    };
    struct Closing final {
        MediaAvSourceDrainRequest request;
        bool opening = true;
        bool cancelled = false;
    };
    struct Stream final {
        Stream(const MediaPreparedRtpSourceSessionStreamPlan& value,
            std::shared_ptr<MediaPreparationMemoryResource> owner,
            std::shared_ptr<MediaPreparationStorageBudget> payload)
            : resource(std::move(owner)), payloadBudget(std::move(payload)), plan(value), pending(resource.get()), spareLeases(resource.get()),
              protocol(nullptr, MediaLineageObjectDeleter<MediaRtpInputProtocolSession>{MediaPreparedLineageAllocation{resource}}),
              binder(nullptr, MediaLineageObjectDeleter<MediaRtpPacketClockSession>{MediaPreparedLineageAllocation{resource}}),
              prefix(makeMediaLineageUnique<MediaPreparedRtpPrefix>(MediaPreparedLineageAllocation{resource}, resource)),
              packets(resource.get()), controls(resource.get()) {}
        std::shared_ptr<MediaPreparationMemoryResource> resource;
        std::shared_ptr<MediaPreparationStorageBudget> payloadBudget;
        // m_plan is constructed before streams and destroyed after them.
        const MediaPreparedRtpSourceSessionStreamPlan& plan;
        // Pending payload fees outlive the protocol assembly they cover.
        std::pmr::vector<std::shared_ptr<MediaPreparationStorageLease>> pending;
        std::pmr::vector<std::shared_ptr<MediaPreparationStorageLease>> spareLeases;
        MediaLineageUniquePtr<MediaRtpInputProtocolSession> protocol;
        MediaLineageUniquePtr<MediaRtpPacketClockSession> binder;
        MediaLineageUniquePtr<MediaPreparedRtpPrefix> prefix;
        std::pmr::deque<MediaPreparedRtpOutput> packets;
        std::pmr::deque<MediaPreparedRtpOutput> controls;
        std::shared_ptr<MediaNodeWakeup> wakeup;
        std::shared_ptr<MediaNodeWakeup> controlWakeup;
        std::shared_ptr<MediaInputActivity> activity;
        std::uint64_t nextSequence = 1;
        std::uint64_t ordinal = 0;
        std::size_t admittedUnits = 0;
        std::size_t outputBytes = 0;
        std::size_t outputHighWater = 0;
        std::size_t packetBytes = 0;
        std::size_t controlBytes = 0;
        std::size_t packetHighWater = 0;
        std::size_t controlHighWater = 0;
        std::optional<MediaPreparedRtpCreditBlocker> waitingCredit;
        std::optional<std::uint64_t> runtimeIngressGeneration;
        std::optional<std::uint32_t> runtimeSsrc;
        std::optional<std::uint64_t> lastControlSequence;
        std::optional<std::uint64_t> consumedControlSequence;
        std::optional<std::uint64_t> consumedRetirementSequence;
        std::optional<MediaRtpClockInvalidation> consumedRetirement;
        // Exclusive user-space capture fence, not an accepted packet frontier.
        std::optional<std::int64_t> rtpCaptureExclusiveBeforeNs;
        ClosingMember closing;
        std::optional<MediaAvGenerationPurge> lastPurge;
        std::optional<std::uint64_t> purgeFrontier;
    };
    ::media::Result<Stream*> stream(MediaStreamKind kind);
    ::media::Status advanceRtpCaptureBoundary(Stream& stream, std::int64_t observedAtNs);
    ::media::Status validateIngressLocked(const Stream& stream, std::span<const std::uint8_t> bytes,
        std::int64_t observedAtNs, std::uint64_t ordinal) const;
    bool controlReady(Stream& stream);
    bool controlFits(const Stream& stream, const MediaRtpPreparedClockObservation& prepared) const;
    ::media::Result<std::optional<MediaPreparedRtpIngressProgress>> consumeDeparture(
        MediaStreamKind kind, std::span<const std::uint8_t> bytes, std::int64_t observedAtNs, std::uint64_t ordinal);
    ::media::Status progressClosing(MediaStreamKind kind, std::int64_t now);
    std::optional<MediaRtpClosedClockMember> clockBoundary(const Stream& stream) const;
    std::int64_t clockEvaluationTime(const Stream& stream, std::int64_t now) const;
    const MediaAvSourceDrainMember& closingMember(const Stream& stream) const;
    bool finishRetirementsLocked();
    ::media::Status progressRetirementsLocked(std::int64_t now);
    bool generationBlocked(const Stream& stream, MediaRtpUdpChannel channel, std::int64_t observedAtNs) const;
    void notifyCredit(Stream& stream) noexcept;
    ::media::Status drain(Stream& stream, std::int64_t now);
    ::media::Status evidence(Stream& stream, std::int64_t now);
    ::media::Status commitControl(Stream& stream, MediaRtpPreparedClockObservation prepared, std::int64_t now, const MediaRtpDiscontinuity* discontinuity);
    ::media::Status event(Stream& stream, MediaBufferRef event,
        bool selectedClock, std::int64_t now);
    ::media::Status queue(Stream& stream, MediaPreparedRtpOutputPort port, MediaBufferRef buffer);
    ::media::Status publish(std::int64_t now);
    ::media::Status bindAvailable(Stream& stream);
    ::media::Status tickLocked(Stream& stream, std::int64_t now);
    ::media::Status prepareFreezeLocked();
    ::media::Result<MediaPreparedRtpStorageSnapshot> storageSnapshotLocked() const;
    ::media::Status validateStorageAdmissionLocked(const MediaPreparedRtpStorageAdmission& product,
        const MediaPreparedRtpStorageSnapshot& snapshot, std::size_t batchStorageBytes) const;
    void applyStorageAdmissionLocked(MediaPreparedRtpStorageAdmission product) noexcept;
    // Initialize fallible synchronization before transferring covered ownership.
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    // Includes plan/configuration storage and must outlive all covered objects.
    WorkingSet m_workingSet;
    MediaPreparedRtpSourceSessionPlan m_plan;
    MediaPreparedRtpSourceBudgets m_budgets;
    std::shared_ptr<MediaMasterClock> m_clock;
    std::shared_ptr<MediaPreparationMemoryResource> m_ownerResource;
    MediaLineageUniquePtr<MediaRtpClockGroupSession> m_group;
    MediaLineageUniquePtr<Stream> m_video, m_audio;
    std::optional<std::uint64_t> m_runtimeClockGeneration;
    bool m_runtimeClockLocked = false;
    std::optional<std::weak_ptr<MediaAvSyncGroupRuntime>> m_runtimeGroup;
    std::optional<Closing> m_closing;
    bool m_committingRetirement = false;
    std::optional<::media::ErrorInfo> m_error;
    std::uint64_t m_revision = 0;
    bool m_frozen = false;
    bool m_bootstrap = true;
    std::int64_t m_bootstrapObservedAtNs = 0;
};

} // namespace media::ffmpeg::graph
