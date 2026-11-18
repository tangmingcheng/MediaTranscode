#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/model/MediaDecoderInputRetention.h"

#include <cstdint>
#include <span>
#include <string>
#include <optional>

struct AVCodecParameters;

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
    std::optional<MediaDecoderInputRetention> preparedInputRetention;
    friend bool operator==(const MediaSelectedAudioDecoder&, const MediaSelectedAudioDecoder&) = default;
};

class MediaAudioDecoderCapabilityProvider final {
public:
    static ::media::Result<MediaSelectedAudioDecoder> verifyDemuxedStream(
        const AVCodecParameters& codecParameters);
    static ::media::Result<MediaSelectedAudioDecoder> verifyAacAudioSpecificConfig(
        int inputSampleRate, int channels,
        std::span<const std::uint8_t> audioSpecificConfig);
    static ::media::Result<MediaSelectedAudioDecoder> verifyAacAdts(
        const AVCodecParameters& codecParameters);
    static ::media::Result<MediaSelectedAudioDecoder> verifyOpusRtp(
        int inputSampleRate, int channels,
        std::int64_t maximumAccessUnitSamples);

private:
    MediaAudioDecoderCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
