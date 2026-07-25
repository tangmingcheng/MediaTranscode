#include "internal/graph/sync/lineage/MediaEncodedAudioCanonicalizerState.h"

namespace media::ffmpeg::graph {

MediaEncodedAudioCanonicalizerState::MediaEncodedAudioCanonicalizerState() noexcept
    : MediaAudioLineageState(true, 1)
{
}

void MediaEncodedAudioCanonicalizerState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    clearOwnedState();
}

void MediaEncodedAudioCanonicalizerState::resetForLifecycle() noexcept
{
    auto stateLock = lock();
    clearOwnedState();
    resetLifecycleLineage();
}

void MediaEncodedAudioCanonicalizerState::clearOwnedState() noexcept
{
    expectedNextSample.reset();
    pending.reset();
    nextSequence = 1;
}

} // namespace media::ffmpeg::graph
