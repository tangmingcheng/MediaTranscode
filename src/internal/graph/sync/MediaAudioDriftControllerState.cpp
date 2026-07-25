#include "internal/graph/sync/MediaAudioDriftControllerState.h"

namespace media::ffmpeg::graph {

MediaAudioDriftControllerState::MediaAudioDriftControllerState() noexcept
    : MediaAudioLineageState(true, 2)
{
}

void MediaAudioDriftControllerState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    clearOwnedState();
}

void MediaAudioDriftControllerState::resetForLifecycle() noexcept
{
    auto stateLock = lock();
    clearOwnedState();
    resetLifecycleLineage();
}

void MediaAudioDriftControllerState::clearOwnedState() noexcept
{
    servo.reset();
    projection.reset();
    origin.reset();
    pending.reset();
    nextSequence = 1;
}

} // namespace media::ffmpeg::graph
