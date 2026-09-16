#pragma once

#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"

#include <optional>

namespace media::ffmpeg::graph {

::media::Status authorizeProtocolPurgeInput(
    const MediaBufferRef& buffer, const MediaProtocolOutputSessionKey& session,
    const MediaAvGenerationPurge& purge,
    const std::optional<MediaAvGenerationPurge>& previousPurge);

} // namespace media::ffmpeg::graph
