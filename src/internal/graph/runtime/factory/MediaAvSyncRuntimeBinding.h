#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
};

} // namespace media::ffmpeg::graph
