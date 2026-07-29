#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvStartupVideoPreparationState;

enum class MediaAvSyncBindingAssemblyMode {
    ComponentCore,
    ProductionProtocolOutput
};

struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaAvGenerationTransitionPlan transition;
    MediaAvSyncBindingAssemblyMode assemblyMode;
    std::optional<MediaAvSyncOutputAdapterKind> outputAdapter;
    std::shared_ptr<MediaAvStartupVideoPreparationState>
        videoPreparationState;
};

} // namespace media::ffmpeg::graph
