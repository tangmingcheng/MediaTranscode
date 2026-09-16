#pragma once

#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvStartupVideoPreparationState;

struct MediaAvRuntimeDomainState final {
    MediaAvSyncGroupKey groupKey;
    MediaAvRuntimeRegistrationPlan registration;
    std::optional<MediaPlaybackEpochActivationCapability> activation;
    MediaAvReacquisitionAssemblyDependencies reacquisition;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparation;
};

} // namespace media::ffmpeg::graph
