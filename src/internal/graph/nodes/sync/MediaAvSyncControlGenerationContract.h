#pragma once

#include "internal/graph/model/MediaControlGenerationPolicy.h"
#include "internal/graph/nodes/MediaOutputCommitReservation.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

enum class MediaControlGenerationDisposition : std::uint8_t {
    Forward = 0,
    Withhold = 1,
    DropOld = 2
};

enum class MediaControlConsumerGenerationRequirement : std::uint8_t {
    None = 0,
    ExactWhenPresent = 1
};

struct MediaAvSyncControlClassification final {
    MediaControlBufferClassification control;
    MediaControlGenerationDisposition generation;
    std::optional<MediaAvGenerationPublicationReservation>
        publicationAuthority;
};

std::string_view mediaControlGenerationPolicyOption(
    MediaControlGenerationPolicy policy) noexcept;

::media::Result<MediaControlGenerationPolicy>
decodeMediaControlGenerationPolicy(std::string_view option);

::media::Result<MediaAvSyncControlClassification>
classifyMediaAvSyncControl(
    const MediaBufferRef& buffer,
    MediaControlGenerationPolicy policy,
    MediaAvSyncGroupRuntime& syncGroup,
    std::uint64_t plannedInitialGeneration);

::media::Result<MediaOutputCommitReservation>
reserveMediaAvSyncControlPublication(
    MediaAvSyncControlClassification classification,
    std::optional<std::uint64_t> exactConsumerGeneration,
    MediaControlConsumerGenerationRequirement consumerRequirement);

} // namespace media::ffmpeg::graph
