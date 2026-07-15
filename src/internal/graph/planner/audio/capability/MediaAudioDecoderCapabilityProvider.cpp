#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"

#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
}

#include <cstring>
#include <vector>

namespace media::ffmpeg::graph {

::media::Result<MediaSelectedAudioDecoder>
MediaAudioDecoderCapabilityProvider::verifyAac(
    int inputSampleRate, int channels,
    std::span<const std::uint8_t> audioSpecificConfig)
{
    const std::vector<std::uint8_t> config(
        audioSpecificConfig.begin(), audioSpecificConfig.end());
    auto parsed = parseAacAudioSpecificConfig(config);
    if (!parsed || inputSampleRate <= 0 || channels <= 0 ||
        parsed.value().sampleRate != inputSampleRate ||
        parsed.value().channels != channels || parsed.value().frameSamples <= 0) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            parsed ? ::media::ErrorInfo::invalidArgument(
                         "AAC decoder capability conflicts with selected source format")
                   : parsed.error());
    }
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!decoder || !decoder->name) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::unsupported("selected AAC decoder is unavailable"));
    }
    auto context = ::media::ffmpeg::makeCodecContext(decoder);
    if (!context) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::allocationFailed("AAC decoder capability allocation failed"));
    }
    context->sample_rate = inputSampleRate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_default(&context->ch_layout, channels);
#else
    context->channels = channels;
    context->channel_layout = av_get_default_channel_layout(channels);
#endif
    context->extradata = static_cast<std::uint8_t*>(
        av_mallocz(audioSpecificConfig.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!context->extradata) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::allocationFailed("AAC decoder extradata allocation failed"));
    }
    std::memcpy(context->extradata, audioSpecificConfig.data(), audioSpecificConfig.size());
    context->extradata_size = static_cast<int>(audioSpecificConfig.size());
    const int opened = avcodec_open2(context.get(), decoder, nullptr);
    if (opened < 0 || context->delay < 0 || context->sample_rate <= 0 ||
        context->sample_fmt == AV_SAMPLE_FMT_NONE) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            opened < 0 ? FFmpegGraphError::fromCode(opened, "AAC decoder capability open")
                       : ::media::ErrorInfo::unsupported(
                             "selected AAC decoder does not publish timing and output format"));
    }
    const char* sampleFormat = av_get_sample_fmt_name(context->sample_fmt);
#if LIBAVUTIL_VERSION_MAJOR >= 57
    char layout[128]{};
    const int layoutStatus = av_channel_layout_describe(
        &context->ch_layout, layout, sizeof(layout));
    const int outputChannels = context->ch_layout.nb_channels;
#else
    char layout[128]{};
    av_get_channel_layout_string(layout, sizeof(layout), context->channels,
                                 context->channel_layout);
    const int layoutStatus = 0;
    const int outputChannels = context->channels;
#endif
    if (!sampleFormat || layoutStatus < 0 || outputChannels <= 0) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::unsupported(
                "selected AAC decoder output format is not representable"));
    }
    return ::media::Result<MediaSelectedAudioDecoder>::success(
        MediaSelectedAudioDecoder{
            decoder->name, sampleFormat, layout, inputSampleRate,
            context->sample_rate, outputChannels, context->delay,
            parsed.value().frameSamples});
}

} // namespace media::ffmpeg::graph
