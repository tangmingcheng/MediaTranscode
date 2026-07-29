#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"

namespace media::ffmpeg::graph {

class MediaAvGenerationTransitionPlanner final {
public:
    static MediaAvGenerationTransitionPlan plan(
        MediaAvSyncOutputAdapterKind adapter,
        MediaAvSyncSourceClockMode sourceClockMode,
        MediaRunningTime acknowledgementTimeout,
        MediaRunningTime terminalDrainWindow);
};

} // namespace media::ffmpeg::graph
