#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"

#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"
#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/protocol/rtp/MediaOpusRtpCapability.h"
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
namespace {

::media::Result<MediaSelectedAudioDecoder> verifyOpenedDecoder(
    AVCodecID codecId,
    int inputSampleRate,
    int channels,
    std::int64_t maximumAccessUnitSamples,
    const AVCodecParameters* codecParameters,
    std::span<const std::uint8_t> decoderConfiguration)
{
    if (codecId == AV_CODEC_ID_NONE || inputSampleRate <= 0 || channels <= 0 ||
        maximumAccessUnitSamples <= 0) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio decoder capability request is incomplete"));
    }
    const AVCodec* decoder = avcodec_find_decoder(codecId);
    if (!decoder || !decoder->name) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::unsupported("selected audio decoder is unavailable"));
    }
    auto context = ::media::ffmpeg::makeCodecContext(decoder);
    if (!context) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::allocationFailed(
                "audio decoder capability allocation failed"));
    }
    if (codecParameters) {
        const int copied = avcodec_parameters_to_context(
            context.get(), codecParameters);
        if (copied < 0) {
            return ::media::Result<MediaSelectedAudioDecoder>::failure(
                FFmpegGraphError::fromCode(
                    copied, "audio decoder capability parameter copy"));
        }
    } else {
        context->sample_rate = inputSampleRate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        av_channel_layout_default(&context->ch_layout, channels);
#else
        context->channels = channels;
        context->channel_layout = av_get_default_channel_layout(channels);
#endif
    }
    if (!decoderConfiguration.empty()) {
        context->extradata = static_cast<std::uint8_t*>(av_mallocz(
            decoderConfiguration.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!context->extradata) {
            return ::media::Result<MediaSelectedAudioDecoder>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "audio decoder extradata allocation failed"));
        }
        std::memcpy(context->extradata, decoderConfiguration.data(),
                    decoderConfiguration.size());
        context->extradata_size = static_cast<int>(decoderConfiguration.size());
    }
    const int opened = avcodec_open2(context.get(), decoder, nullptr);
    if (opened < 0 || context->delay < 0 || context->sample_rate <= 0 ||
        context->sample_fmt == AV_SAMPLE_FMT_NONE) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            opened < 0
                ? FFmpegGraphError::fromCode(
                      opened, "selected audio decoder capability open")
                : ::media::ErrorInfo::unsupported(
                      "selected audio decoder does not publish timing and output format"));
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
    if (!sampleFormat || layoutStatus < 0 || outputChannels <= 0 ||
        context->sample_rate != inputSampleRate || outputChannels != channels) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::unsupported(
                "selected audio decoder output format conflicts with source"));
    }
    return ::media::Result<MediaSelectedAudioDecoder>::success(
        MediaSelectedAudioDecoder{
            decoder->name, sampleFormat, layout, inputSampleRate,
            context->sample_rate, outputChannels, context->delay,
            maximumAccessUnitSamples});
}

} // namespace

::media::Result<MediaSelectedAudioDecoder>
MediaAudioDecoderCapabilityProvider::verifyAacAudioSpecificConfig(
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
    return verifyOpenedDecoder(
        AV_CODEC_ID_AAC, inputSampleRate, channels,
        parsed.value().frameSamples, nullptr, audioSpecificConfig);
}

::media::Result<MediaSelectedAudioDecoder>
MediaAudioDecoderCapabilityProvider::verifyAacAdts(
    const AVCodecParameters& codecParameters)
{
    if (codecParameters.codec_type != AVMEDIA_TYPE_AUDIO ||
        codecParameters.codec_id != AV_CODEC_ID_AAC ||
        codecParameters.profile != AV_PROFILE_AAC_LOW ||
        codecParameters.sample_rate <= 0) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS ADTS decoder capability requires selected AAC-LC parameters"));
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int channels = codecParameters.ch_layout.nb_channels;
#else
    const int channels = codecParameters.channels;
#endif
    auto configuration = makeMediaAacLcLongFrameAudioSpecificConfig(
        codecParameters.sample_rate, channels);
    if (!configuration) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            configuration.error());
    }
    return verifyAacAudioSpecificConfig(
        codecParameters.sample_rate, channels, configuration.value());
}

::media::Result<MediaSelectedAudioDecoder>
MediaAudioDecoderCapabilityProvider::verifyOpusRtp(
    int inputSampleRate, int channels,
    std::int64_t maximumAccessUnitSamples)
{
    if (inputSampleRate != 48'000) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Opus RTP decoder capability requires a 48000 Hz clock"));
    }
    if (auto status = validateOpusRtpMappingFamilyZeroChannels(channels);
        !status) {
        return ::media::Result<MediaSelectedAudioDecoder>::failure(
            status.error());
    }
    return verifyOpenedDecoder(
        AV_CODEC_ID_OPUS, inputSampleRate, channels,
        maximumAccessUnitSamples, nullptr, {});
}

} // namespace media::ffmpeg::graph
