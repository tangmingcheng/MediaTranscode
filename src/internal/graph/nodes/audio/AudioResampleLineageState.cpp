#include "internal/graph/nodes/audio/AudioResampleLineageState.h"

namespace media::ffmpeg::graph {

AudioResampleLineageState::AudioResampleLineageState(
    MediaAudioLineageExecutionMode mode,
    std::size_t capacity) noexcept
    : MediaAudioLineageState(
          mode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
          capacity)
{
}

void AudioResampleLineageState::resetForLifecycle() noexcept
{
    swr.reset();
    correctionExecutor.reset();
    clearLineageStorage();
    resetLifecycleLineage();
}

void AudioResampleLineageState::clearLineageStorage() noexcept
{
    nextOutputPts = AV_NOPTS_VALUE;
    outputSampleIndex = 0;
    pendingOutputs.clear();
    pendingInput.reset();
    pendingTerminal.reset();
    drainingEof = false;
    drainingClosedInput = false;
    lifecycleFlushRequested = false;
    preferCorrection = true;
    activeOrigin.reset();
    outputIntervals.reset();
    sampleProjection.reset();
    lastOutputLineage.reset();
    terminals.reset();
    eofEmitted = false;
}

void AudioResampleLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge& purge) noexcept
{
    swr.reset();
    if (correctionExecutor) {
        const std::uint64_t generation =
            correctionExecutor->mode() ==
                    MediaAudioCorrectionExecutionMode::Disabled
                ? 0
                : purge.nextGeneration;
        (void)correctionExecutor->reset(generation);
    }
    clearLineageStorage();
}

} // namespace media::ffmpeg::graph
