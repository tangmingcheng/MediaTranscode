#pragma once

#include "internal/graph/model/MediaVideoSourceTiming.h"
#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbe.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include <span>
#include "internal/graph/model/MediaRtpVideoTimingPlan.h"

namespace media::ffmpeg::graph {

class MediaRtpSourceProbeGoalPlanner final {
public:
    static ::media::Result<MediaRtpSourceProbeGoal> plan(
        const MediaVideoSourceTiming& timing, const std::optional<MediaRtpVideoTimingPlan>& plannedTiming,
        std::span<const MediaBufferRef> snapshot,
        const std::shared_ptr<MediaPreparationStorageBudget>& budget);
};

} // namespace media::ffmpeg::graph
