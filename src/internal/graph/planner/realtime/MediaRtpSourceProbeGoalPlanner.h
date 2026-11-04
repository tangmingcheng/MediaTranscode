#pragma once

#include "internal/graph/model/MediaVideoSourceTiming.h"
#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbe.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include <span>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaRtpSourceProbeGoalPlanner final {
public:
    static ::media::Result<MediaRtpSourceProbeGoal> plan(
        const MediaVideoSourceTiming& timing, std::string_view source,
        std::span<const MediaBufferRef> snapshot,
        const std::shared_ptr<MediaPreparationStorageBudget>& budget);
};

} // namespace media::ffmpeg::graph
