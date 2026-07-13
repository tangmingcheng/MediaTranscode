#include "internal/graph/nodes/audio/AudioCodecResolverNode.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegInputSnapshotBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

::media::Result<std::optional<int>> intOption(const MediaNodeOptions* options, const std::string& key)
{
    if (!options) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }
    const std::string value = options->value(key);
    if (value.empty()) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }
    int parsed = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return ::media::Result<std::optional<int>>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode invalid integer option: " + key));
    }
    return ::media::Result<std::optional<int>>::success(parsed);
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }
    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

bool sampleFormatSupported(const AVCodec* encoder, AVSampleFormat format)
{
    if (!encoder || !encoder->sample_fmts) {
        return true;
    }
    for (const AVSampleFormat* current = encoder->sample_fmts; *current != AV_SAMPLE_FMT_NONE; ++current) {
        if (*current == format) {
            return true;
        }
    }
    return false;
}

AVSampleFormat chooseSampleFormat(const AVCodec* encoder, AVSampleFormat preferred)
{
    if (preferred != AV_SAMPLE_FMT_NONE && sampleFormatSupported(encoder, preferred)) {
        return preferred;
    }
    if (encoder && encoder->sample_fmts && encoder->sample_fmts[0] != AV_SAMPLE_FMT_NONE) {
        return encoder->sample_fmts[0];
    }
    return preferred == AV_SAMPLE_FMT_NONE ? AV_SAMPLE_FMT_FLTP : preferred;
}

bool sampleRateSupported(const AVCodec* encoder, int sampleRate)
{
    if (!encoder || !encoder->supported_samplerates || sampleRate <= 0) {
        return true;
    }
    for (const int* current = encoder->supported_samplerates; *current != 0; ++current) {
        if (*current == sampleRate) {
            return true;
        }
    }
    return false;
}

::media::Result<int> chooseSampleRate(const AVCodec* encoder, int requested, int source)
{
    const int preferred = requested > 0 ? requested : source;
    if (preferred <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires known audio sample rate"));
    }
    if (sampleRateSupported(encoder, preferred)) {
        return ::media::Result<int>::success(preferred);
    }
    return ::media::Result<int>::failure(
        ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode audio sample rate is not supported by selected encoder"));
}

::media::Result<int64_t> bitrateBitsFromKbps(int kbps, const char* name)
{
    if (kbps <= 0) {
        return ::media::Result<int64_t>::failure(::media::ErrorInfo::invalidArgument(std::string(name) + " must be positive"));
    }
    constexpr int64_t kBitsPerKbps = 1000;
    if (kbps > std::numeric_limits<int64_t>::max() / kBitsPerKbps) {
        return ::media::Result<int64_t>::failure(::media::ErrorInfo::invalidArgument(std::string(name) + " is too large"));
    }
    return ::media::Result<int64_t>::success(static_cast<int64_t>(kbps) * kBitsPerKbps);
}

::media::Result<int> bufferBitsFromKbits(int kbits)
{
    if (kbits <= 0) {
        return ::media::Result<int>::failure(::media::ErrorInfo::invalidArgument("audio buffer size must be positive"));
    }
    constexpr int kBitsPerKbit = 1000;
    if (kbits > std::numeric_limits<int>::max() / kBitsPerKbit) {
        return ::media::Result<int>::failure(::media::ErrorInfo::invalidArgument("audio buffer size is too large"));
    }
    return ::media::Result<int>::success(kbits * kBitsPerKbit);
}

const AVCodec* findEncoder(const MediaNodeOptions* options)
{
    const std::string plannedEncoder = optionValue(options, MediaTranscodeOptionKey::PlannedEncoder);
    if (!plannedEncoder.empty()) {
        return avcodec_find_encoder_by_name(plannedEncoder.c_str());
    }

    const std::string codecName = optionValue(options, MediaTranscodeOptionKey::AudioCodec);
    if (!codecName.empty()) {
        if (const AVCodec* byName = avcodec_find_encoder_by_name(codecName.c_str())) {
            return byName;
        }
        if (const AVCodecDescriptor* descriptor = avcodec_descriptor_get_by_name(codecName.c_str())) {
            return avcodec_find_encoder(descriptor->id);
        }
    }
    return nullptr;
}

} // namespace

AudioCodecResolverNode::AudioCodecResolverNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioCodecResolverNode")
{
}

MediaNodeKind AudioCodecResolverNode::staticKind() noexcept
{
    return MediaNodeKind::AudioCodecResolver;
}

::media::Result<MediaNodeProcessResult> AudioCodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    auto* formatBuffer = dynamic_cast<FFmpegInputSnapshotBuffer*>(input.value()->get());

    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode expected complete input snapshots"));
    }

    auto stream = resolveSourceStream(context, *formatBuffer);
    if (!stream) {
        return ::media::Result<MediaNodeProcessResult>::failure(stream.error());
    }

    auto decoder = buildDecoderContext(*stream.value());
    if (!decoder) {
        return ::media::Result<MediaNodeProcessResult>::failure(decoder.error());
    }

    auto encoder = buildEncoderContext(context, *stream.value(), decoder.value().get());
    if (!encoder) {
        return ::media::Result<MediaNodeProcessResult>::failure(encoder.error());
    }

    auto decoderStatus = emitCodecContext(context, "decoder", std::move(decoder).value());
    if (!decoderStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(decoderStatus.error());
    }

    auto encoderStatus = emitCodecContext(context, "encoder", std::move(encoder).value());
    if (!encoderStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(encoderStatus.error());
    }

    m_emitted = true;
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
}

::media::Result<const FFmpegInputStreamSnapshot*> AudioCodecResolverNode::resolveSourceStream(
    MediaGraphExecutionContext& context,
    const FFmpegInputSnapshotBuffer& format) const
{
    auto streamIndexOption = intOption(nodeOptions(context), MediaTranscodeOptionKey::AudioSourceStreamIndex);
    if (!streamIndexOption) {
        return ::media::Result<const FFmpegInputStreamSnapshot*>::failure(streamIndexOption.error());
    }
    if (!streamIndexOption.value() || *streamIndexOption.value() < 0) {
        return ::media::Result<const FFmpegInputStreamSnapshot*>::failure(::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires audio source stream index"));
    }
    const int streamIndex = *streamIndexOption.value();
    const auto* stream = format.inputStreamSnapshot(streamIndex);
    if (!stream || stream->streamKind != MediaStreamKind::Audio) {
        return ::media::Result<const FFmpegInputStreamSnapshot*>::failure(::media::ErrorInfo::invalidArgument("AudioCodecResolverNode source stream is not audio"));
    }
    return ::media::Result<const FFmpegInputStreamSnapshot*>::success(stream);
}

::media::Result<::media::ffmpeg::CodecContextPtr> AudioCodecResolverNode::buildDecoderContext(const FFmpegInputStreamSnapshot& stream) const
{
    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(codecParameters.error());
    }
    const AVCodec* decoder = avcodec_find_decoder(codecParameters.value()->codec_id);
    if (!decoder) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::unsupported("AudioCodecResolverNode audio decoder not found"));
    }
    auto context = ::media::ffmpeg::makeCodecContext(decoder);
    if (!context) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::allocationFailed("AudioCodecResolverNode failed to allocate decoder context"));
    }
    const int copyRet = avcodec_parameters_to_context(context.get(), codecParameters.value().get());
    if (copyRet < 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            FFmpegGraphError::fromCode(copyRet, "avcodec_parameters_to_context(audio decoder)"));
    }
    context->pkt_timebase = AVRational{ stream.time.timeBase.num, stream.time.timeBase.den };
    const int openRet = avcodec_open2(context.get(), decoder, nullptr);
    if (openRet < 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            FFmpegGraphError::fromCode(openRet, "avcodec_open2(audio decoder)"));
    }
    return ::media::Result<::media::ffmpeg::CodecContextPtr>::success(std::move(context));
}

::media::Result<::media::ffmpeg::CodecContextPtr> AudioCodecResolverNode::buildEncoderContext(
    MediaGraphExecutionContext& context,
    const FFmpegInputStreamSnapshot& stream,
    const AVCodecContext* decoderContext) const
{
    auto codecParameters = stream.cloneCodecParameters();
    if (!codecParameters) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(codecParameters.error());
    }
    const MediaNodeOptions* options = nodeOptions(context);
    const AVCodec* encoder = findEncoder(options);
    if (!encoder) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::unsupported("AudioCodecResolverNode audio encoder not found"));
    }

    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::allocationFailed("AudioCodecResolverNode failed to allocate encoder context"));
    }

    auto requestedSampleRate = intOption(options, MediaTranscodeOptionKey::AudioSampleRate);
    if (!requestedSampleRate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(requestedSampleRate.error());
    }
    const int sourceSampleRate = decoderContext ? decoderContext->sample_rate : codecParameters.value()->sample_rate;
    auto targetSampleRate = chooseSampleRate(encoder,
                                             requestedSampleRate.value().value_or(0),
                                             sourceSampleRate);
    if (!targetSampleRate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(targetSampleRate.error());
    }

    encoderContext->sample_rate = targetSampleRate.value();
    encoderContext->sample_fmt = chooseSampleFormat(encoder, decoderContext ? decoderContext->sample_fmt : AV_SAMPLE_FMT_NONE);
    encoderContext->time_base = AVRational{ 1, targetSampleRate.value() };

    auto requestedChannels = intOption(options, MediaTranscodeOptionKey::AudioChannels);
    if (!requestedChannels) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(requestedChannels.error());
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (requestedChannels.value()) {
        if (*requestedChannels.value() <= 0) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode rejects non-positive audio channel count"));
        }
        av_channel_layout_default(&encoderContext->ch_layout, *requestedChannels.value());
    } else if (decoderContext && decoderContext->ch_layout.nb_channels > 0) {
        const int ret = av_channel_layout_copy(&encoderContext->ch_layout, &decoderContext->ch_layout);
        if (ret < 0) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                FFmpegGraphError::fromCode(ret, "av_channel_layout_copy(audio encoder)"));
        }
    } else if (codecParameters.value()->ch_layout.nb_channels > 0) {
        const int ret = av_channel_layout_copy(&encoderContext->ch_layout, &codecParameters.value()->ch_layout);
        if (ret < 0) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                FFmpegGraphError::fromCode(ret, "av_channel_layout_copy(audio encoder codecpar)"));
        }
    } else {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires known audio channel layout"));
    }
#else
    if (requestedChannels.value()) {
        encoderContext->channels = *requestedChannels.value();
        encoderContext->channel_layout = av_get_default_channel_layout(*requestedChannels.value());
    } else if (decoderContext && decoderContext->channels > 0) {
        encoderContext->channels = decoderContext->channels;
        encoderContext->channel_layout = decoderContext->channel_layout ? decoderContext->channel_layout : av_get_default_channel_layout(decoderContext->channels);
    } else if (codecParameters.value()->channels > 0) {
        encoderContext->channels = codecParameters.value()->channels;
        encoderContext->channel_layout = codecParameters.value()->channel_layout ? codecParameters.value()->channel_layout : av_get_default_channel_layout(codecParameters.value()->channels);
    } else {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires known audio channels"));
    }
#endif

    auto bitrate = intOption(options, MediaTranscodeOptionKey::AudioBitrateKbps);
    if (!bitrate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bitrate.error());
    }
    if (bitrate.value()) {
        auto bits = bitrateBitsFromKbps(*bitrate.value(), "audio bitrate");
        if (!bits) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bits.error());
        }
        encoderContext->bit_rate = bits.value();
    } else if (codecParameters.value()->bit_rate > 0) {
        encoderContext->bit_rate = codecParameters.value()->bit_rate;
    }

    auto minBitrate = intOption(options, MediaTranscodeOptionKey::AudioMinBitrateKbps);
    if (!minBitrate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(minBitrate.error());
    }
    if (minBitrate.value()) {
        auto bits = bitrateBitsFromKbps(*minBitrate.value(), "audio min bitrate");
        if (!bits) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bits.error());
        }
        encoderContext->rc_min_rate = bits.value();
    }

    auto maxBitrate = intOption(options, MediaTranscodeOptionKey::AudioMaxBitrateKbps);
    if (!maxBitrate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(maxBitrate.error());
    }
    if (maxBitrate.value()) {
        auto bits = bitrateBitsFromKbps(*maxBitrate.value(), "audio max bitrate");
        if (!bits) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bits.error());
        }
        encoderContext->rc_max_rate = bits.value();
    }
    if (encoderContext->rc_min_rate > 0 && encoderContext->rc_max_rate > 0 && encoderContext->rc_min_rate > encoderContext->rc_max_rate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires audio min bitrate <= max bitrate"));
    }

    auto bufferSize = intOption(options, MediaTranscodeOptionKey::AudioBufferSizeKbits);
    if (!bufferSize) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bufferSize.error());
    }
    if (bufferSize.value()) {
        auto bits = bufferBitsFromKbits(*bufferSize.value());
        if (!bits) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(bits.error());
        }
        encoderContext->rc_buffer_size = bits.value();
    }

    setPrivateOption(encoderContext.get(), "preset", optionValue(options, MediaTranscodeOptionKey::AudioPreset));
    setPrivateOption(encoderContext.get(), "profile", optionValue(options, MediaTranscodeOptionKey::AudioProfile));

    auto quality = intOption(options, MediaTranscodeOptionKey::AudioQuality);
    if (!quality) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(quality.error());
    }
    if (quality.value()) {
        if (*quality.value() < 0) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode rejects negative audio quality"));
        }
        setPrivateOption(encoderContext.get(), "q", std::to_string(*quality.value()));
        setPrivateOption(encoderContext.get(), "quality", std::to_string(*quality.value()));
    }

    const int openRet = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (openRet < 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            FFmpegGraphError::fromCode(openRet, "avcodec_open2(audio encoder)"));
    }

    return ::media::Result<::media::ffmpeg::CodecContextPtr>::success(std::move(encoderContext));
}

::media::Status AudioCodecResolverNode::emitCodecContext(MediaGraphExecutionContext& context,
                                                         const char* portName,
                                                         ::media::ffmpeg::CodecContextPtr codecContext)
{
    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(codecContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    buffer.value()->setStreamKind(MediaStreamKind::Audio);
    buffer.value()->setPayloadKind(MediaPayloadKind::CodecContext);
    if (auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.value().get())) {
        if (AVCodecContext* codec = codecBuffer->context()) {
            buffer.value()->setFormatDescriptor(FFmpegDescriptorMapper::fromCodecContext(codec, MediaCodecOperation::Unknown));
            MediaTimeDescriptor time;
            time.timeBase = MediaRational{ codec->time_base.num, codec->time_base.den };
            buffer.value()->setTimeDescriptor(time);
        }
    }
    return emitOutput(context, portName, buffer.value());
}

} // namespace media::ffmpeg::graph
