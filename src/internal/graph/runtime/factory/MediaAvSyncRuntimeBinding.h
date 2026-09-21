#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvStartupVideoPreparationState;
struct MediaAvContinuousAggregatePlan;

struct MediaAvSyncComponentCoreRuntimeProduct final {};

enum class MediaSynchronizedAudioExecutionProduct : std::uint8_t {
    PacketCopy = 0,
    FrameTranscode = 1
};

using MediaAvSyncRuntimeOutputProduct = std::variant<
    MediaAvSyncComponentCoreRuntimeProduct,
    MediaSeparateRtpOutputRuntimePlan,
    MediaProjectMpegTsRuntimeOutputPlan>;

struct MediaAvSharedSourceOutputDomainBinding final {
    MediaAvGenerationTransitionPlan transition;
    MediaAvRuntimeRegistrationPlan registration;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparationState;
};

struct MediaAvSourceDomainRegistration final {
    MediaAvRuntimeInputRegistration input;
    MediaNodeId preparationOwner;
    std::vector<MediaNodeId> processingMembers;
};

struct MediaAvSourceDomainBinding final {
    MediaAvGenerationTransitionPlan transition;
    MediaAvSourceDomainRegistration registration;
    std::shared_ptr<MediaAvStartupVideoPreparationState> videoPreparationState;
};

struct MediaAvOutputDomainRegistration final {
    MediaNodeId activationOwner;
    MediaNodeId outputScheduler;
    std::optional<MediaNodeId> rtpSdpPublisher;
    std::vector<MediaNodeId> processingMembers;
};

struct MediaAvOutputDomainBinding final {
    MediaAvOutputDomainRegistration registration;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    MediaBufferRef preparedVideoEncoder;
};

using MediaAvRuntimeDomainRole = std::variant<
    MediaAvSharedSourceOutputDomainBinding,
    MediaAvSourceDomainBinding,
    MediaAvOutputDomainBinding>;

struct MediaAvRuntimeDomainBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaAvRuntimeDomainRole role;
};

struct MediaAvSyncRuntimeBinding final {
    std::vector<MediaAvRuntimeDomainBinding> domains;
    MediaAvSyncGroupKey outputGroupKey;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaDatagramTransportPlanTemplate datagramTransport;
    MediaSynchronizedAudioExecutionProduct audioExecutionProduct;
    MediaAvSyncRuntimeOutputProduct outputProduct;
};

} // namespace media::ffmpeg::graph
