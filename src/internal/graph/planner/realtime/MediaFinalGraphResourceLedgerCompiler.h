#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaEncoderHardwareFramesPoolPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupNodeMediaPlanner.h"
#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"
#include "internal/graph/model/MediaGraphPayloadProducerFact.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaSourceFrameOwnershipStoragePlanner.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
struct MediaSourceLineageStoragePlan;
struct MediaPreparedPacketWrapperStoragePlan;
struct MediaPreparedFrameWrapperStoragePlan;

enum class MediaFinalGraphResourceScope : std::uint8_t {
    EngineManagedPayloadAndReservedStorage = 1,
    ObservedOnlyExternalAllocation = 2,
    AccountedByNetworkLedger = 3,
    ObservedOnlyDeviceAndDriverAllocation = 4
};

enum class MediaFinalGraphReferenceKind { Unspecified, None, Node, Edge, RetainingGroup };

struct MediaFinalGraphResourceLedgerEntry final {
    std::string owner;
    std::string sharedAllocationGroup;
    MediaFinalGraphResourceScope scope;
    std::uint64_t payloadBytes;
    std::uint64_t queueSlots;
    std::uint64_t maximumBufferObjects;
    std::uint64_t retainedItems;
    bool coveredByGlobalPayloadLedger;
    std::string authority;
    // A shared reservation is admitted only with its complete retaining group.
    // Empty means graph-wide storage (e.g. graph services), not a guessed owner.
    std::vector<MediaNodeId> retentionNodes;
    std::shared_ptr<const void> storageAllocationOwner;
    // Exact ordinary reference identity; owner text is diagnostic only.
    std::optional<MediaNodeId> referenceNode;
    std::optional<MediaEdgeId> referenceEdge;
    MediaFinalGraphReferenceKind referenceKind = MediaFinalGraphReferenceKind::Unspecified;
    bool validReferenceIdentity() const noexcept {
        switch(referenceKind){
        case MediaFinalGraphReferenceKind::None:return !referenceNode && !referenceEdge && retentionNodes.empty();
        case MediaFinalGraphReferenceKind::Node:return referenceNode && referenceNode->isValid() && !referenceEdge && retentionNodes.empty();
        case MediaFinalGraphReferenceKind::Edge:return referenceEdge && referenceEdge->isValid() && !referenceNode && retentionNodes.empty();
        case MediaFinalGraphReferenceKind::RetainingGroup:return !referenceNode && !referenceEdge && !retentionNodes.empty();
        default:return false;
        }
    }
};

struct MediaFinalGraphResourceLedger final {
    MediaRealtimeGraphResourceBudgetScope resourceScope;
    std::uint64_t maximumGraphPayloadAndReservedStorageBytes;
    std::uint64_t admittedGraphPayloadAndReservedStorageBytes;
    std::uint64_t admittedDeviceAndDriverBytes;
    std::vector<MediaFinalGraphResourceLedgerEntry> entries;
    std::vector<std::string> outOfScopeAuthorities;
    std::optional<MediaEncoderHardwareFramesPoolPlan> encoderFramesPool;
    MediaGraphPayloadCreditPlan payloadCreditPlan;
    std::uint64_t videoPipelinePendingSurfaces;
    std::uint64_t terminalReferenceSlots;
    std::uint64_t terminalControlObjects;
    std::vector<MediaStartupEventMediaBound> startupEventMedia;
    std::vector<MediaStartupNodeMediaBound> startupNodeMedia;
    MediaProtocolGenerationStoragePlan protocolGenerationStorage;
};

struct MediaFinalGraphReferenceStoragePlan final {
    std::uint64_t reservedStorageBytes;
    std::vector<MediaFinalGraphResourceLedgerEntry> entries;
    std::vector<MediaStartupEventMediaBound> startupEventMedia;
    std::vector<MediaStartupNodeMediaBound> startupNodeMedia;
    MediaProtocolGenerationStoragePlan protocolGenerationStorage;
};

// Frozen allocation facts for one graph, independent of the single-source
// planning ledger. The budget is produced before final retention admission.
struct MediaSourceFrameOwnershipStorageEvidence final {
    std::size_t sourceIndex;
    MediaNodeId decoder;
    std::optional<MediaNodeId> filter;
    std::shared_ptr<const void> allocationOwner;
    MediaSourceFrameOwnershipStoragePlan storage;
    std::shared_ptr<const MediaSourceLineageStoragePlan> lineageStorage;
    std::vector<std::shared_ptr<const MediaPreparedPacketWrapperStoragePlan>> packetWrappers;
    std::shared_ptr<const MediaPreparedFrameWrapperStoragePlan> frameWrappers;
};
struct MediaFinalGraphResourceAdmissionPlan final {
    MediaRealtimeGraphResourceBudgetScope resourceScope;
    std::uint64_t maximumGraphPayloadAndReservedStorageBytes;
    std::uint64_t maximumEncoderRetainedFrames;
    std::optional<std::uint64_t> maximumAudioEncoderRetainedFrames;
    std::optional<MediaPreparedHardwareMemoryEnvelope> hardwareMemory;
    bool hardwareEncoderSurfacePool;
    std::optional<MediaEncoderHardwareFramesPoolPlan> encoderFramesPool;
    std::vector<MediaGraphPayloadProducerFact> producers;
    std::vector<MediaFinalGraphResourceLedgerEntry> fixedStorage;
    std::vector<MediaGraphPayloadCreditAccountPlan> accounts;
    std::vector<std::string> producerAccounts;
    std::vector<std::uint64_t> minimumProgressObjects;
    // Already contained in preparation reservations, never added a second time.
    // Its owner-capacity check is necessary, not complete pre-freeze admission.
    std::vector<MediaSourceFrameOwnershipStorageEvidence> sourceFrameOwnership;
};

class MediaFinalGraphResourceLedgerCompiler final {
public:
    static ::media::Result<MediaFinalGraphReferenceStoragePlan> compileReferenceStorage(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
        std::span<const MediaNodeId> selectedNodes);
    static ::media::Result<MediaFinalGraphResourceLedger> compile(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
        std::span<const MediaNodeId> selectedNodes);
    static ::media::Result<MediaFinalGraphReferenceStoragePlan> compileReferenceStorage(
        const MediaGraph& graph, const MediaFinalGraphResourceAdmissionPlan& facts);
    static ::media::Result<MediaFinalGraphResourceLedger> compile(
        const MediaGraph& graph,
        const MediaFinalGraphResourceAdmissionPlan& admission);
};

} // namespace media::ffmpeg::graph
