#pragma once

#include "internal/graph/model/MediaDatagramServiceScopeContract.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"

namespace media::ffmpeg::graph {

class MediaDatagramServiceScopePlanner final {
public:
    static ::media::Result<MediaDatagramServiceScopeContract> plan(
        const MediaDatagramTransportPlanTemplate& transport);
private:
    MediaDatagramServiceScopePlanner() = delete;
};

} // namespace media::ffmpeg::graph
