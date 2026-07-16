#pragma once

#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

#include <cstdint>
#include <deque>
#include <optional>

extern "C" {
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

struct AudioResamplePendingInput final {
    MediaBufferRef buffer;
    bool submitted = false;
};

class AudioResampleLineageState final : public MediaAudioLineageState {
public:
    AudioResampleLineageState(MediaAudioLineageExecutionMode mode,
                              std::size_t capacity) noexcept;

    ::media::ffmpeg::SwrContextPtr swr;
    std::optional<AudioSwrCompensationExecutor> correctionExecutor;
    int64_t nextOutputPts = AV_NOPTS_VALUE;
    std::int64_t outputSampleIndex = 0;
    std::deque<MediaBufferRef> pendingOutputs;
    std::optional<AudioResamplePendingInput> pendingInput;
    MediaBufferRef pendingTerminal;
    bool drainingEof = false;
    bool drainingClosedInput = false;
    bool lifecycleFlushRequested = false;
    bool preferCorrection = true;
    std::optional<MediaAudioPlaybackOrigin> activeOrigin;
    MediaAudioIntervalAccumulator outputIntervals;
    std::optional<MediaAudioSampleProjection> sampleProjection;
    std::shared_ptr<const MediaCanonicalLineage> lastOutputLineage;
    MediaInputTerminalTracker terminals { { "frame" } };
    bool eofEmitted = false;

    void resetForLifecycle() noexcept;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearLineageStorage() noexcept;
};

} // namespace media::ffmpeg::graph
