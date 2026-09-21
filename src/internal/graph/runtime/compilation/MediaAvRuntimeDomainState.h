#pragma once

#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/sync/MediaOutputEpochActivationCapability.h"

#include <memory>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaAvSourceDomainRuntimeState final {
    MediaAvSourceDomainRegistration registration;
    std::optional<MediaPlaybackEpochActivationCapability> activation;
    MediaAvReacquisitionAssemblyDependencies reacquisition;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparation;
};

struct MediaAvSharedSourceOutputDomainRuntimeState final {
    MediaAvRuntimeRegistrationPlan registration;
    std::optional<MediaPlaybackEpochActivationCapability> activation;
    MediaAvReacquisitionAssemblyDependencies reacquisition;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparation;
};

struct MediaAvOutputDomainRuntimeState final {
    MediaAvOutputDomainRegistration registration;
    std::optional<MediaOutputEpochActivationCapability> activation;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    MediaBufferRef preparedVideoEncoder;
};

struct MediaAvRuntimeDomainState final {
    MediaAvSyncGroupKey groupKey;
    std::variant<MediaAvSharedSourceOutputDomainRuntimeState,
                 MediaAvSourceDomainRuntimeState,
                 MediaAvOutputDomainRuntimeState> role;
};

} // namespace media::ffmpeg::graph
