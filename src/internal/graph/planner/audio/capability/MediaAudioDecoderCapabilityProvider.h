#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

struct MediaSelectedAudioDecoder final {
    std::string name;
    std::string outputSampleFormat;
    std::string outputChannelLayout;
    int inputSampleRate;
    int outputSampleRate;
    int outputChannels;
    std::int64_t delayOutputSamples;
    std::int64_t maximumOutputBlockInputSamples;
};

class MediaAudioDecoderCapabilityProvider final {
public:
    static ::media::Result<MediaSelectedAudioDecoder> verifyAac(
        int inputSampleRate, int channels,
        std::span<const std::uint8_t> audioSpecificConfig);

private:
    MediaAudioDecoderCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
