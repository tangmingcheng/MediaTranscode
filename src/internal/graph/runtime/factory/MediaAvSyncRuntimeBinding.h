#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaAvGenerationTransitionPlan transition;
};

} // namespace media::ffmpeg::graph
