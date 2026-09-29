#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"

namespace media::ffmpeg::graph {

// True accepts an exact terminal; false discards an authorized obsolete one.
inline ::media::Result<bool> classifyProtocolTerminalGeneration(
    const MediaControlBuffer& control, std::uint64_t activeGeneration,
    MediaTranscodeStreamSet streamSet,
    const std::optional<MediaAvGenerationPurge>& completedPurge)
{
    if (control.controlKind() == MediaControlBufferKind::Abort)
        return ::media::Result<bool>::failure(::media::ErrorInfo::cancelled("Protocol input aborted"));
    const auto generation = control.generation();
    if (generation && *generation == 0) return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument("Protocol terminal generation cannot be zero"));
    if (generation && completedPurge && *generation <= completedPurge->oldGeneration)
        return ::media::Result<bool>::success(false);
    if (control.controlKind() != MediaControlBufferKind::Eof || activeGeneration == 0 ||
        (generation ? *generation != activeGeneration : streamSet != MediaTranscodeStreamSet::VideoOnly))
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Protocol terminal requires its exact active generation"));
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
