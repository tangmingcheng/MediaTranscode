#pragma once

namespace media::ffmpeg::graph {

class AudioResampleSwrSession;

enum class AudioSwrDrainEvidence {
    NoDelay,
    MayProduce,
};

class AudioSwrResamplerExhausted final {
public:
    AudioSwrResamplerExhausted(
        const AudioSwrResamplerExhausted&) noexcept = default;
    AudioSwrResamplerExhausted& operator=(
        const AudioSwrResamplerExhausted&) noexcept = default;

private:
    AudioSwrResamplerExhausted() noexcept = default;
    friend class AudioResampleSwrSession;
};

} // namespace media::ffmpeg::graph
