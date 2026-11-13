#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedPacketWrapperStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedFrameWrapperStoragePlanner.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaSourceFrameOwnershipStoragePlanner.h"

namespace media::ffmpeg::graph {
struct MediaPreparedStreamCandidateCostFacts final {
    MediaPreparedRtpContainerBounds containers;
};
struct MediaPreparedSourceCandidateCostFacts final {
    MediaPreparedStreamCandidateCostFacts video;
    std::optional<MediaPreparedStreamCandidateCostFacts> audio;
    std::size_t startupVideoUnits;
    std::optional<std::size_t> startupAudioUnits;
    const MediaVideoSourcePlan& videoPlan;
    const MediaCompositionGraphTopology& topology;
    const MediaCompositionReferenceResidencePlan& residence;
    std::size_t sourceIndex;
    std::uint64_t registryCapacity;
    const MediaStartupRoutePlan& startupRoute;
    std::size_t groupKeyCharacters;
    const MediaCompositionProducerHolderFacts& producerHolders;
    MediaRtpControlDelivery controlDelivery;
};
enum class MediaPreparedPayloadLeaseRootKind {
    Prefix, Acquiring, Outputs, ProtocolPending, RawInputCreditWait, BinderCreditWait, Count
};
struct MediaPreparedPayloadLeaseRoot final {
    MediaPreparedPayloadLeaseRootKind kind;
    std::size_t maximumControls;
};
struct MediaPreparedPayloadStoragePlan final {
    std::size_t maximumLiveReservations;
    std::size_t maximumReservationBytes;
    std::size_t bytes;
};
struct MediaPreparedStreamCandidateCosts final {
    // Original control field remains available for freeze validation, but is
    // excluded from containerBytes and replaced by all-live leaseControlsBytes.
    MediaPreparedRtpContainerStoragePlan containers;
    std::size_t containerBytes;
    std::size_t leaseControlsBytes;
    std::optional<MediaPreparedPacketWrapperStoragePlan> packetWrappers;
    std::array<MediaPreparedPayloadLeaseRoot,
        static_cast<std::size_t>(MediaPreparedPayloadLeaseRootKind::Count)> leaseControlRoots;
    MediaPreparedPayloadStoragePlan payload;
};
struct MediaPreparedIngressEventStoragePlan final {
    std::size_t maximumLiveWrappers;
    std::size_t bytes;
};
struct MediaPreparedSourceCandidateCosts final {
    std::size_t fixedBytes;
    MediaPreparedStreamCandidateCosts video;
    std::optional<MediaPreparedStreamCandidateCosts> audio;
    std::size_t startupIndexBytes;
    std::size_t startupGenerationBytes;
    MediaPreparedFrameWrapperStoragePlan frameWrappers;
    std::optional<MediaSourceLineageStoragePlan> lineage;
    MediaCompositionStartupStoragePlan startup;
    MediaSourceFrameOwnershipStoragePlan frameOwnership;
    // Source tracker/mapper/binders and source output evidence, separate from
    // startup.clock's additional graph-route CNAME owners.
    MediaRtcpPreparationStoragePlan sourceClock;
    MediaPreparedIngressEventStoragePlan ingressEvents;
    // Source-owned preparation charges only. Batch scratch and other root
    // reservations are combined once by the batch solver, not by each source.
    // Ordinary route requests and graph payload/registry domains are separate.
    std::size_t protocolBytes;
    std::size_t startupBytes;
    std::size_t totalBytes;
};
class MediaPreparedSourceCandidateCostPlanner final {
public:
    // Reuses catalogs from the same topology, producer and projection domains.
    // Failure invalidates the partially updated candidate.
    static ::media::Status rebind(
        const MediaPreparedRtpSourceSessionPlan& session,
        const MediaPreparedRtpStorageSnapshot& held,
        const MediaPreparedSourceCandidateCostFacts& facts, MediaPreparedSourceCandidateCosts& costs);
    static ::media::Result<MediaPreparedSourceCandidateCosts> plan(
        const MediaPreparedRtpSourceSessionPlan& session,
        const MediaPreparedRtpStorageSnapshot& held,
        const MediaPreparedSourceCandidateCostFacts& facts);
};
}
