#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"

namespace media::ffmpeg::graph {

struct MediaSelectedAudioResampler final {
    int inputSampleRate;
    int outputSampleRate;
    std::int64_t maximumInputBlockSamples;
    std::int64_t maximumOutputBlockSamples;
};

class MediaAudioResamplerCapabilityProvider final {
public:
    static ::media::Result<MediaSelectedAudioResampler> verify(
        const MediaSelectedAudioDecoder& decoder,
        const MediaResolvedAudioOutputPlan& output);

private:
    MediaAudioResamplerCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
