#pragma once
#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionTopologyPlanningKernel.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedSourceCandidateCostPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionResourcePlanner.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"

namespace media::ffmpeg::graph {
// One fixed-shape candidate workspace. Original prepared source facts and the
// output are borrowed synchronously; all retained catalogs own their backing.
// Original video and session-plan owners must outlive every workspace use.
// Failed refresh invalidates the complete workspace; it is not budget pressure.
class MediaCompositionCandidateWorkspace final {
    // Must outlive the route strings below. No move assignment is permitted.
    std::optional<MediaPreparationStorageLease> m_routeBacking;
    const std::shared_ptr<MediaPreparationMemoryResource> m_resource;
    const MediaCompositionOutputResidence m_outputResidence;
public:
    MediaCompositionCandidateWorkspace(MediaCompositionCandidateWorkspace&&) = default;
    MediaCompositionCandidateWorkspace& operator=(MediaCompositionCandidateWorkspace&&) = delete;
    MediaCompositionCandidateWorkspace(const MediaCompositionCandidateWorkspace&) = delete;
    MediaCompositionCandidateWorkspace& operator=(const MediaCompositionCandidateWorkspace&) = delete;
    // The lock-held caller must supply sources/prepared/capacities/held in
    // the original session order. Ingress geometry is not source identity;
    // this workspace preserves that association, it cannot discover it.
    static ::media::Result<MediaCompositionCandidateWorkspace> create(
        MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> sources,
        std::span<const MediaCompositionSourceFixedRequest> prepared,
        std::span<const MediaCompositionSourceCapacityCandidate> capacities,
        std::span<const MediaPreparedRtpStorageSourceSnapshot> held,
        const MediaRealtimeAvOutputRuntimePlan& output, const MediaVideoCanvasGeometry& canvas,
        MediaRational cadence, std::size_t audioSource,
        const MediaPreparedRealtimeEmissionSet& emission,
        const MediaRealtimeDeploymentLatencyBudget& latency,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
    ::media::Status rebind(
        std::span<const MediaCompositionSourceFixedRequest> prepared,
        std::span<const MediaCompositionSourceCapacityCandidate> capacities);
    bool valid() const noexcept { return m_valid; }
    std::size_t sourceBytes() const noexcept { return m_sourceBytes; }

    // Consumption is allowed only after successful independent sealed validation.
    // Moves preserve fixed-capacity element addresses until final binding.
    MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> sources;
    std::optional<MediaAvContinuousAggregateTopology> aggregate;
    std::optional<MediaCompositionTopologyPlanningProducts> topology;
    std::optional<MediaCompositionReferenceResidencePlan> residence;
    MediaFixedCapacityStorage<MediaPreparedSourceCandidateCosts> costs;
private:
    MediaCompositionCandidateWorkspace(MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> sourceProducts,
        std::shared_ptr<MediaPreparationMemoryResource> resource, MediaCompositionOutputResidence outputResidence);
    ::media::Status project(
        std::span<const MediaCompositionSourceFixedRequest> prepared,
        std::span<const MediaCompositionSourceCapacityCandidate> capacities, bool initial);
    ::media::Status price(
        std::span<const MediaCompositionSourceCapacityCandidate> capacities, bool initial);
    MediaFixedCapacityStorage<MediaPreparedRtpStorageSourceSnapshot> m_held;
    MediaFixedCapacityStorage<MediaAvAggregateSourceFacts> m_aggregateFacts;
    MediaFixedCapacityStorage<MediaCompositionTopologySourceFacts> m_topologyFacts;
    MediaFixedCapacityStorage<MediaCompositionSourceResidenceFacts> m_residenceFacts;
    bool m_valid = false;
    std::size_t m_sourceBytes = 0;
};
}
