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

::media::Result<void> setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires complete private codec option"));
    }
    const int status = av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
    if (status < 0) return ::media::Result<void>::failure(
        FFmpegGraphError::fromCode(status, "av_opt_set(audio encoder " + key + ")"));
    return ::media::Result<void>::success();
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

::media::Result<AVSampleFormat> plannedSampleFormat(const AVCodec* encoder,
                                                    const MediaNodeOptions* options)
{
    const std::string name = optionValue(options, MediaTranscodeOptionKey::AudioSampleFormat);
    const AVSampleFormat format = av_get_sample_fmt(name.c_str());
    if (name.empty() || format == AV_SAMPLE_FMT_NONE || !sampleFormatSupported(encoder, format)) {
        return ::media::Result<AVSampleFormat>::failure(
            ::media::ErrorInfo::unsupported("AudioCodecResolverNode planned sample format is missing or unsupported"));
    }
    return ::media::Result<AVSampleFormat>::success(format);
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

::media::Result<int> chooseSampleRate(const AVCodec* encoder, int requested)
{
    if (requested <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires known audio sample rate"));
    }
    if (sampleRateSupported(encoder, requested)) {
        return ::media::Result<int>::success(requested);
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
    return plannedEncoder.empty() ? nullptr : avcodec_find_encoder_by_name(plannedEncoder.c_str());
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

    auto encoder = buildEncoderContext(context, *stream.value());
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
    const FFmpegInputStreamSnapshot& stream) const
{
    static_cast<void>(stream);
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
    auto targetSampleRate = chooseSampleRate(
        encoder, requestedSampleRate.value().value_or(0));
    if (!targetSampleRate) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(targetSampleRate.error());
    }

    encoderContext->sample_rate = targetSampleRate.value();
    auto sampleFormat = plannedSampleFormat(encoder, options);
    if (!sampleFormat) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(sampleFormat.error());
    }
    encoderContext->sample_fmt = sampleFormat.value();
    encoderContext->time_base = AVRational{ 1, targetSampleRate.value() };

    auto requestedChannels = intOption(options, MediaTranscodeOptionKey::AudioChannels);
    if (!requestedChannels) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(requestedChannels.error());
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const std::string plannedLayout = optionValue(options, MediaTranscodeOptionKey::AudioChannelLayout);
    if (!requestedChannels.value() || *requestedChannels.value() <= 0 || plannedLayout.empty()) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires planned audio channels and channel layout"));
    }
    {
        const int layoutStatus = av_channel_layout_from_string(
            &encoderContext->ch_layout, plannedLayout.c_str());
        if (layoutStatus < 0 || encoderContext->ch_layout.nb_channels != *requestedChannels.value()) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode planned channel layout is invalid or inconsistent"));
        }
    }
#else
    if (!requestedChannels.value() || *requestedChannels.value() <= 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires planned audio channels"));
    }
    encoderContext->channels = *requestedChannels.value();
    encoderContext->channel_layout = av_get_default_channel_layout(*requestedChannels.value());
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

    const std::string profile = optionValue(options, MediaTranscodeOptionKey::AudioProfile);
    if (profile.empty() || profile == "unknown") {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode requires resolved audio profile"));
    }
    if (profile == "aac_low") {
        encoderContext->profile = AV_PROFILE_AAC_LOW;
    } else if (profile != "not_applicable") {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::unsupported("AudioCodecResolverNode planned audio profile is unsupported"));
    }
    const std::string preset = optionValue(options, MediaTranscodeOptionKey::AudioPreset);
    if (!preset.empty()) {
        if (auto status = setPrivateOption(encoderContext.get(), "preset", preset); !status) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(status.error());
        }
    }

    auto quality = intOption(options, MediaTranscodeOptionKey::AudioQuality);
    if (!quality) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(quality.error());
    }
    if (quality.value()) {
        if (*quality.value() < 0) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                ::media::ErrorInfo::invalidArgument("AudioCodecResolverNode rejects negative audio quality"));
        }
        if (auto status = setPrivateOption(encoderContext.get(), "q", std::to_string(*quality.value())); !status) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(status.error());
        }
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
