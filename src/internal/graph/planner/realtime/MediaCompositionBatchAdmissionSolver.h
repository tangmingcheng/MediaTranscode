#pragma once
#include "internal/graph/planner/realtime/MediaCompositionCandidateWorkspace.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"

namespace media::ffmpeg::graph {
// Single-use, lock-held negotiation over one immutable source/output set.
// The caller preserves source order and every borrowed prepared/output owner.
class MediaCompositionBatchAdmissionSolver final : public MediaPreparedRtpBatchAdmissionSolver {
public:
    MediaCompositionBatchAdmissionSolver(
        std::span<const MediaCompositionSourceFixedRequest> prepared,
        MediaFixedCapacityStorage<MediaCompositionSourceFixedProducts>& fixed,
        const MediaRealtimeAvOutputRuntimePlan& output, const MediaVideoCanvasGeometry& canvas,
        MediaRational cadence, std::size_t audioSource,
        const MediaPreparedRealtimeEmissionSet& emission,
        const MediaRealtimeDeploymentLatencyBudget& latency,
        std::shared_ptr<MediaPreparationMemoryResource> root, MediaPreparationControl control);
    ::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>> solve(
        std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
        const MediaPreparedRtpBatchBudgetView& budgets,
        const MediaPreparedRtpBatchStoragePlan& batchStorage) const override;
    // Products remain borrowed until the caller independently validates sealed
    // source capacities/costs. Only then may it consume the workspace products.
    MediaCompositionCandidateWorkspace* workspace() noexcept;
    std::span<const MediaCompositionSourceCapacityCandidate> capacities() const;
    std::optional<std::size_t> selectedIncrement() const noexcept;
private:
    enum class Membership { Legal, Gap, UpperBound };
    ::media::Status check(std::span<const MediaPreparedRtpStorageSourceSnapshot> sources) const;
    ::media::Result<Membership> select(std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
        std::size_t increment) const;
    ::media::Result<std::size_t> upperBound(std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
        std::size_t available) const;
    const std::span<const MediaCompositionSourceFixedRequest> m_prepared;
    MediaFixedCapacityStorage<MediaCompositionSourceFixedProducts>& m_fixed;
    const MediaRealtimeAvOutputRuntimePlan& m_output;
    const MediaVideoCanvasGeometry& m_canvas;
    const MediaRational m_cadence;
    const std::size_t m_audioSource;
    const MediaPreparedRealtimeEmissionSet& m_emission;
    const MediaRealtimeDeploymentLatencyBudget& m_latency;
    const std::shared_ptr<MediaPreparationMemoryResource> m_root;
    const MediaPreparationControl m_control;
    mutable std::optional<MediaFixedCapacityStorage<MediaCompositionSourceCapacityCandidate>> m_capacities;
    mutable std::optional<MediaCompositionCandidateWorkspace> m_workspace;
    mutable std::optional<std::size_t> m_selected;
    mutable bool m_attempted = false;
    mutable bool m_solved = false;
};
}
