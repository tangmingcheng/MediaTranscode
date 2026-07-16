#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"
#include "media_transcode/Result.h"

#include <memory>
#include <optional>
#include <cstdint>

struct AVCodecContext;
struct AVFrame;

namespace media::ffmpeg::graph {

class AudioResampleLineageState;

struct AudioResampleSwrLiveConversion final {
    ::media::ffmpeg::FramePtr output;
    int capacity = 0;
    int produced = 0;
};

struct AudioResampleSwrDrainConversion final {
    ::media::ffmpeg::FramePtr output;
    int capacity = 0;
    int produced = 0;
    std::optional<AudioSwrResamplerExhausted> exhausted;
};

class AudioResampleSwrSession final {
public:
    explicit AudioResampleSwrSession(
        std::shared_ptr<AudioResampleLineageState> state) noexcept;

    bool initialized() const noexcept;
    bool frameMatchesTarget(
        const AVFrame& input,
        const AVCodecContext& target) const noexcept;
    ::media::Status ensureInitialized(
        const AVFrame& input,
        const AVCodecContext& target);
    ::media::Result<AudioResampleSwrLiveConversion> convertLive(
        const uint8_t** inputData,
        int inputSamples,
        int maximumOutputSamples,
        const AVCodecContext& target);
    ::media::Result<AudioSwrDrainEvidence> inspectDrainEvidence(
        int inputSampleRate,
        int outputSampleRate) const;
    ::media::Result<AudioResampleSwrDrainConversion> drainQuantum(
        int maximumOutputSamples,
        const AVCodecContext& target);

private:
    ::media::Result<::media::ffmpeg::FramePtr> allocateOutputFrame(
        int capacity,
        const AVCodecContext& target) const;
    std::shared_ptr<AudioResampleLineageState> m_state;
};

} // namespace media::ffmpeg::graph
