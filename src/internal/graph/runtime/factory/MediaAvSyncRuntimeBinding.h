#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <memory>
#include <variant>

namespace media::ffmpeg::graph {

class MediaAvStartupVideoPreparationState;

struct MediaAvSyncComponentCoreRuntimeProduct final {};

using MediaAvSyncRuntimeOutputProduct = std::variant<
    MediaAvSyncComponentCoreRuntimeProduct,
    MediaSeparateRtpOutputRuntimePlan,
    MediaProjectMpegTsRuntimeOutputPlan>;

struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaAvGenerationTransitionPlan transition;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaAvSyncRuntimeOutputProduct outputProduct;
    std::shared_ptr<MediaAvStartupVideoPreparationState>
        videoPreparationState;
};

} // namespace media::ffmpeg::graph
