#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"

namespace media::ffmpeg::graph {

class MediaAvGenerationTransitionPlanner final {
public:
    static MediaAvGenerationTransitionPlan plan(
        MediaAvSyncOutputAdapterKind adapter,
        MediaRunningTime acknowledgementTimeout,
        MediaRunningTime terminalDrainWindow);
};

} // namespace media::ffmpeg::graph
