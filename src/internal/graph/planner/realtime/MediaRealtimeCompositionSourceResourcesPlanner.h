#pragma once

#include "internal/graph/model/MediaRealtimeCompositionSourceTargets.h"
#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/model/MediaGraphPayloadProducerFact.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"

namespace media::ffmpeg::graph {

struct MediaCompositionInputAllocationFacts final {
    MediaNodeId nodeId;
    MediaPreparedInputPayloadEnvelope payload;
    MediaRtpIngressPlan ingress;
};

// Codec-owned inputs and atomic producer completion are not graph queue or
// startup-node storage. Add only these owners to the final graph traversal.
struct MediaCompositionProducerResidenceFact final {
    MediaGraphPayloadProducerKey key;
    std::uint64_t additionalRetainedObjects;
    std::uint64_t minimumProgressObjects;
    std::string authority;
};

struct MediaCompositionFrameOwnershipFacts final {
    MediaNodeId decoder;
    std::optional<MediaNodeId> filter;
    MediaVideoFrameOwnershipStoragePlan decoderGeometry;
    std::optional<MediaVideoFrameOwnershipStoragePlan> filterGeometry;
    std::shared_ptr<const void> allocationOwner;
    MediaVideoFrameProducerFacts decoderReadiness;
    std::optional<MediaVideoFrameProducerFacts> filterReadiness;
};

// Per-source allocation facts, not simultaneous retention or global admission.
// Candidate references held by the aggregate retain their source allocation.
struct MediaRealtimeCompositionSourceResources final {
    std::size_t sourceIndex;
    std::vector<MediaCompositionInputAllocationFacts> inputs;
    std::vector<MediaGraphPayloadProducerFact> producers;
    std::vector<MediaCompositionProducerResidenceFact> residence;
    std::vector<MediaPreparedRtpStorageReservation> preparationStorage;
    std::vector<MediaNodeId> preparationOwners;
    std::optional<MediaCompositionFrameOwnershipFacts> frameOwnership;
    std::optional<MediaCompositionSourceResidenceFacts> referenceFacts;
    std::optional<std::uint64_t> sourceLineageRegistryCapacity;
    std::optional<MediaHardwareTransferDirection> sourceTransferDirection;
    std::optional<MediaPreparedRtpSourceContainerBounds> preparedPacketContainers;
    std::optional<MediaCompositionProducerHolderFacts> packetStorageHolders;
};

class MediaRealtimeCompositionSourceResourcesPlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionSourceResources> plan(
        const MediaGraph& graph, const MediaRealtimeCompositionSourcePlan& source,
        const MediaRealtimeCompositionSourceTargets& targets);
};

} // namespace media::ffmpeg::graph
