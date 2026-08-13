#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {

class MediaAvGenerationTransitionPlanner final {
public:
    static MediaAvGenerationTransitionPlan plan(
        MediaAvSyncOutputAdapterKind adapter,
        MediaAvSyncSourceClockMode sourceClockMode,
        MediaBranchMode audioBranchMode,
        bool videoFilterActive,
        MediaRunningTime acknowledgementTimeout,
        MediaRunningTime terminalDrainWindow);
};

} // namespace media::ffmpeg::graph
