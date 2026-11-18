#pragma once
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "internal/graph/model/MediaOutputReservationStoragePlan.h"

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
class MediaPreparedVideoDecoder;
class MediaPreparedVideoCanvas;
class MediaPreparedRtpSourceSession;
class MediaMasterClock;

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
    MediaOutputReservationStoragePlan reservationStorage;
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
    MediaOutputReservationStoragePlan reservationStorage;
    std::shared_ptr<MediaPreparedVideoDecoder> preparedVideoDecoder;
    std::shared_ptr<MediaPreparedRtpSourceSession> preparedRtpSource;
};

struct MediaAvOutputDomainRegistration final {
    MediaNodeId activationOwner;
    MediaNodeId audioAggregate;
    MediaNodeId outputScheduler;
    std::optional<MediaNodeId> rtpSdpPublisher;
    std::vector<MediaNodeId> processingMembers;
};

struct MediaAvOutputDomainBinding final {
    MediaAvOutputDomainRegistration registration;
    std::shared_ptr<const MediaAvContinuousAggregatePlan> aggregatePlan;
    MediaBufferRef preparedVideoEncoder;
    std::shared_ptr<MediaPreparedVideoCanvas> preparedCanvas;
};

using MediaAvRuntimeDomainRole = std::variant<
    MediaAvSharedSourceOutputDomainBinding,
    MediaAvSourceDomainBinding,
    MediaAvOutputDomainBinding>;

struct MediaAvRuntimeDomainBinding final {
    // Survives plan and role destruction; composition transfers its fixed lease.
    std::shared_ptr<const MediaPreparationStorageLease> fixedStorage;
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaAvRuntimeDomainRole role;

    MediaAvSyncDomainRole domainRole() const noexcept
    {
        if (std::holds_alternative<MediaAvSharedSourceOutputDomainBinding>(role))
            return MediaAvSyncDomainRole::SharedSourceOutput;
        if (std::holds_alternative<MediaAvSourceDomainBinding>(role))
            return MediaAvSyncDomainRole::SourceContribution;
        return MediaAvSyncDomainRole::ContinuousOutput;
    }
};

struct MediaAvSyncRuntimeBinding final {
    std::vector<MediaAvRuntimeDomainBinding> domains;
    MediaAvSyncGroupKey outputGroupKey;
    MediaDatagramTransportPlanTemplate datagramTransport;
    MediaSynchronizedAudioExecutionProduct audioExecutionProduct;
    MediaAvSyncRuntimeOutputProduct outputProduct;
    MediaEncodedAccessUnitSchedulingPlan scheduling;
    std::shared_ptr<MediaMasterClock> preparedMasterClock;
};

} // namespace media::ffmpeg::graph
