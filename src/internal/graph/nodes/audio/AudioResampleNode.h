#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"
#include "internal/graph/nodes/audio/AudioResampleLineageMapper.h"
#include "internal/graph/nodes/audio/AudioResampleLineageState.h"
#include "internal/graph/nodes/audio/AudioResampleSwrSession.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

#include <cstdint>
#include <deque>
#include <optional>

extern "C" {
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

class AudioResampleNode final : public FFmpegCodecNodeRuntime {
public:
    using LineageState = AudioResampleLineageState;

    AudioResampleNode(MediaNodeId nodeId, MediaAudioLineageExecutionMode lineageMode,
                      std::shared_ptr<AudioResampleLineageState> lineageState);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    ::media::Status emitTerminal(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& terminal);
    ::media::Status bindEncoderContext(MediaGraphExecutionContext& context);
    ::media::Status configureCorrection(MediaGraphExecutionContext& context);
    ::media::Result<bool> consumeCorrection(MediaGraphExecutionContext& context);
    ::media::Status processFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status processPendingInputQuantum(MediaGraphExecutionContext& context);
    ::media::Status processEofDrainQuantum(MediaGraphExecutionContext& context);
    ::media::Status convertLiveQuantum(MediaGraphExecutionContext& context,
                                       const uint8_t** inputData,
                                       int inputSamples,
                                       std::int64_t inputPts,
                                       AVRational sourceTimeBase);
    ::media::Status drainSwrQuantum(MediaGraphExecutionContext& context,
                                    bool correctionWindowRequired);
    ::media::Status emitNextPending(MediaGraphExecutionContext& context);
    ::media::Status stampAndQueue(MediaBufferRef outputBuffer,
                                  std::int64_t inputPts,
                                  AVRational sourceTimeBase);
    ::media::Status settleLineageResidue();
    void resetRuntimeState() noexcept;

private:
    std::shared_ptr<AudioResampleLineageState> m_lineageState;
    AudioResampleLineageMapper m_lineageMapper;
    AudioResampleSwrSession m_swrSession;
    ::media::ffmpeg::SwrContextPtr& m_swr;
    std::optional<AudioSwrCompensationExecutor>& m_correctionExecutor;
    int64_t& m_nextOutputPts;
    std::int64_t& m_outputSampleIndex;
    std::deque<MediaBufferRef>& m_pendingOutputs;
    std::optional<AudioResamplePendingInput>& m_pendingInput;
    MediaBufferRef& m_pendingTerminal;
    bool& m_drainingEof;
    bool& m_drainingClosedInput;
    bool& m_lifecycleFlushRequested;
    bool& m_preferCorrection;
    MediaAudioLineageExecutionMode m_lineageMode;
    std::optional<MediaAudioPlaybackOrigin>& m_activeOrigin;
    MediaAudioIntervalAccumulator& m_outputIntervals;
    std::optional<MediaAudioSampleProjection>& m_sampleProjection;
    std::shared_ptr<const MediaCanonicalLineage>& m_lastOutputLineage;
};

} // namespace media::ffmpeg::graph
