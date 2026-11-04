#pragma once

#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaOutputEpochActivationCapability.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvContinuousAggregatePlan;
class MediaPreparedVideoCanvas;
class MediaAvAggregateCoordination;

struct MediaAvAggregateSourceRuntime final {
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparation;
};

struct MediaAvAggregateRuntimeDependencies final {
    std::vector<MediaAvAggregateSourceRuntime> sources;
    std::shared_ptr<MediaAvSyncGroupRuntime> output;
    MediaOutputEpochActivationCapability activation;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    std::shared_ptr<MediaPreparedVideoCanvas> preparedCanvas;
    std::shared_ptr<MediaAvAggregateCoordination> coordination;
};

} // namespace media::ffmpeg::graph
