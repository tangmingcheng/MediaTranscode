#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"

#include <cstdint>
#include <deque>
#include <optional>

extern "C" {
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

class AudioResampleNode final : public FFmpegCodecNodeRuntime {
public:
    explicit AudioResampleNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    struct PendingInput final {
        MediaBufferRef buffer;
        bool submitted = false;
    };

    ::media::Status bindEncoderContext(MediaGraphExecutionContext& context);
    ::media::Status configureCorrection(MediaGraphExecutionContext& context);
    ::media::Result<bool> consumeCorrection(MediaGraphExecutionContext& context);
    ::media::Status processFrame(MediaGraphExecutionContext& context, const MediaBufferRef& buffer);
    ::media::Status processPendingInputQuantum(MediaGraphExecutionContext& context);
    ::media::Status processEofDrainQuantum(MediaGraphExecutionContext& context);
    ::media::Status convertQuantum(MediaGraphExecutionContext& context,
                                   const uint8_t** inputData,
                                   int inputSamples,
                                   std::int64_t inputPts,
                                   AVRational sourceTimeBase,
                                   bool liveInput);
    ::media::Status emitNextPending(MediaGraphExecutionContext& context);
    ::media::Status stampAndQueue(MediaBufferRef outputBuffer,
                                  std::int64_t inputPts,
                                  AVRational sourceTimeBase);
    ::media::Status ensureSwrInitialized(const AVFrame* inputFrame);
    bool frameMatchesEncoder(const AVFrame* frame) const noexcept;
    void resetRuntimeState() noexcept;

private:
    ::media::ffmpeg::SwrContextPtr m_swr;
    std::optional<AudioSwrCompensationExecutor> m_correctionExecutor;
    int64_t m_nextOutputPts = AV_NOPTS_VALUE;
    std::int64_t m_outputSampleIndex = 0;
    std::deque<MediaBufferRef> m_pendingOutputs;
    std::optional<PendingInput> m_pendingInput;
    MediaBufferRef m_pendingTerminal;
    bool m_drainingEof = false;
    bool m_drainingClosedInput = false;
    bool m_lifecycleFlushRequested = false;
    MediaInputTerminalTracker m_terminals { { "frame" } };
    bool m_eofEmitted = false;
    bool m_preferCorrection = true;
};

} // namespace media::ffmpeg::graph
