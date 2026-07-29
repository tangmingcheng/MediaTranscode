#pragma once

#include "internal/graph/core/MediaNode.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "media_transcode/Result.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaDemuxTimestampClockMapper;

struct MediaRuntimeGenerationPurgeRegistration final {
    MediaAvGenerationParticipant participant;
    MediaAvGenerationPurgeRegistration registration;
};

class MediaRuntimeNodeFactory final {
public:
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>> create(const MediaNode& node);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>> create(
        const MediaNode& node,
        MediaPreparedRealtimeInputBinding* binding);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>> create(
        const MediaNode& node,
        MediaPreparedRealtimeInputBinding* binding,
        const std::shared_ptr<MediaAvStartupVideoPreparationState>&
            videoPreparationState);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>>
    createActivatedStartupReleaseSequencer(
        const MediaNode& node,
        MediaPlaybackEpochActivationCapability capability,
        const std::shared_ptr<MediaAvStartupVideoPreparationState>&
            videoPreparationState);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>>
    createScheduledRtpSender(
        const MediaNode& node,
        std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>>
    createDemuxPacketClockBinder(
        const MediaNode& node,
        std::shared_ptr<MediaDemuxTimestampClockMapper> mapper,
        std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup);
    static std::optional<MediaRuntimeGenerationPurgeRegistration>
    generationPurgeRegistration(MediaRuntimeNode& node);
    static bool supported(MediaNodeKind kind) noexcept;
};

} // namespace media::ffmpeg::graph
