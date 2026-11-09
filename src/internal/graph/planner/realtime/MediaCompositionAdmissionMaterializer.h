#pragma once
#include "internal/graph/planner/realtime/MediaCompositionSourceCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"
#include "internal/graph/planner/realtime/MediaPreparedSourceCandidateCostPlanner.h"

namespace media::ffmpeg::graph {
class MediaCompositionAdmissionMaterializer final {
public:
    // Inputs must belong to the same source in the lock-held workspace order.
    // Fixed string backing is charged to root, separately from source costs.
    static ::media::Result<MediaPreparedRtpStorageAdmission> create(
        const MediaPreparedRtpStorageSourceSnapshot& held,
        const MediaCompositionSourceCapacityCandidate& candidate,
        const MediaCompositionSourcePlanningProducts& source,
        const MediaPreparedSourceCandidateCosts& costs,
        const std::shared_ptr<MediaPreparationMemoryResource>& root);

    // Failure invalidates totalBytes, so freeze cannot publish the old product.
    // Success changes only numeric products; fixed strings retain their backing.
    static ::media::Status rebind(
        const MediaPreparedRtpStorageSourceSnapshot& held,
        const MediaCompositionSourceCapacityCandidate& candidate,
        const MediaCompositionSourcePlanningProducts& source,
        const MediaPreparedSourceCandidateCosts& costs,
        MediaPreparedRtpStorageAdmission& admission);
};
}
