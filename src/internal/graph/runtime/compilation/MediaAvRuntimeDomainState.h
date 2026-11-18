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
    std::shared_ptr<MediaPreparedVideoDecoder> preparedVideoDecoder;
    std::shared_ptr<MediaPreparedRtpSourceSession> preparedRtpSource;
};

struct MediaAvSharedSourceOutputDomainRuntimeState final {
    MediaAvRuntimeRegistrationPlan registration;
    std::optional<MediaPlaybackEpochActivationCapability> activation;
    MediaAvReacquisitionAssemblyDependencies reacquisition;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparation;
};

class MediaAvAggregateCoordination;

struct MediaAvOutputDomainRuntimeState final {
    MediaAvOutputDomainRegistration registration;
    std::optional<MediaOutputEpochActivationCapability> activation;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    MediaBufferRef preparedVideoEncoder;
    std::shared_ptr<MediaPreparedVideoCanvas> preparedCanvas;
    std::shared_ptr<MediaAvAggregateCoordination> coordination;
};

struct MediaAvRuntimeDomainState final {
    MediaAvSyncGroupKey groupKey;
    std::variant<MediaAvSharedSourceOutputDomainRuntimeState,
                 MediaAvSourceDomainRuntimeState,
                 MediaAvOutputDomainRuntimeState> role;
};

} // namespace media::ffmpeg::graph
