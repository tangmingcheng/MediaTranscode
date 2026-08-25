#include "internal/graph/planner/audio/capability/MediaAudioEncoderCapabilityProvider.h"
#include "internal/graph/planner/audio/capability/MediaAudioEncoderTargetIdentityValidator.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
}

#include <string>
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t BitsPerKilobit = 1000;
constexpr std::int64_t BitsPerByte = 8;

::media::Result<std::int64_t> rateBits(
    const std::optional<int>& kilobits,
    const char* field)
{
    if (!kilobits || *kilobits <= 0 ||
        *kilobits > (std::numeric_limits<std::int64_t>::max)() /
            BitsPerKilobit) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                std::string("audio encoder requires authoritative ") + field));
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(*kilobits) * BitsPerKilobit);
}

std::uint64_t ceilBytes(std::int64_t bits) noexcept
{
    return static_cast<std::uint64_t>(
        bits / BitsPerByte + (bits % BitsPerByte != 0 ? 1 : 0));
}

std::vector<const AVCodec*> encoderCandidates(const std::string& codecName)
{
    std::vector<const AVCodec*> candidates;
    const AVCodec* encoder = avcodec_find_encoder_by_name(codecName.c_str());
    if (encoder) candidates.push_back(encoder);
    const AVCodecDescriptor* descriptor = avcodec_descriptor_get_by_name(codecName.c_str());
    const AVCodecID codecId = descriptor
        ? descriptor->id
        : (encoder ? encoder->id : AV_CODEC_ID_NONE);
    void* opaque = nullptr;
    while (const AVCodec* candidate = av_codec_iterate(&opaque)) {
        if (!av_codec_is_encoder(candidate) || candidate->id != codecId) continue;
        bool duplicate = false;
        for (const AVCodec* existing : candidates) {
            if (existing == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) candidates.push_back(candidate);
    }
    return candidates;
}

::media::Status applyChannelLayout(AVCodecContext& context,
                                   const MediaResolvedAudioTargetDecision& target)
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int status = av_channel_layout_from_string(
        &context.ch_layout, target.channelLayout().c_str());
    if (status < 0 || context.ch_layout.nb_channels != target.channels()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio encoder capability verification requires consistent channel layout"));
    }
#else
    context.channels = target.channels();
    context.channel_layout = av_get_default_channel_layout(target.channels());
    if (context.channel_layout == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio encoder capability verification requires representable channel layout"));
    }
#endif
    return ::media::Status::success();
}

::media::Result<MediaSelectedAudioEncoder> verifyCandidate(
    const AVCodec& encoder,
    const MediaResolvedAudioTargetDecision& target)
{
    if (!encoder.name || !encoder.sample_fmts ||
        encoder.sample_fmts[0] == AV_SAMPLE_FMT_NONE) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported(
                "audio encoder candidate has no usable sample format"));
    }
    const AVSampleFormat sampleFormat = encoder.sample_fmts[0];
    const char* sampleFormatName = av_get_sample_fmt_name(sampleFormat);
    if (!sampleFormatName) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported(
                "audio encoder sample format is unknown: " + target.codecName()));
    }

    auto context = ::media::ffmpeg::makeCodecContext(&encoder);
    if (!context) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::allocationFailed(
                "failed to allocate audio encoder capability context"));
    }
    context->log_level_offset = AV_LOG_MAX_OFFSET;
    context->sample_rate = target.sampleRate();
    context->sample_fmt = sampleFormat;
    context->time_base = AVRational{1, target.sampleRate()};
    context->profile = target.profile().ffmpegProfileId();
    auto targetRate = rateBits(target.bitrateKbps(), "target bitrate");
    auto maximumRate = rateBits(
        target.maxBitrateKbps() ? target.maxBitrateKbps()
                                : target.bitrateKbps(),
        "maximum bitrate");
    if (!targetRate || !maximumRate) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            !targetRate ? targetRate.error() : maximumRate.error());
    }
    context->bit_rate = targetRate.value();
    context->rc_max_rate = maximumRate.value();
    if (target.bufferSizeKbits()) {
        auto buffer = rateBits(target.bufferSizeKbits(), "VBV");
        if (!buffer || buffer.value() > (std::numeric_limits<int>::max)()) {
            return ::media::Result<MediaSelectedAudioEncoder>::failure(
                buffer ? ::media::ErrorInfo::invalidArgument(
                             "audio encoder VBV exceeds AVCodecContext range")
                       : buffer.error());
        }
        context->rc_buffer_size = static_cast<int>(buffer.value());
    }
    if (auto status = applyChannelLayout(*context, target); !status) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(status.error());
    }

    const int openStatus = avcodec_open2(context.get(), &encoder, nullptr);
    if (openStatus < 0) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            FFmpegGraphError::fromCode(
                openStatus,
                "avcodec_open2(audio encoder capability " + target.codecName() + ")"));
    }
    if (auto identity = MediaAudioEncoderTargetIdentityValidator::validate(
            target, sampleFormat, *context); !identity) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(identity.error());
    }

    MediaSelectedAudioEncoder verified;
    verified.name = encoder.name;
    verified.sampleFormat = sampleFormatName;
    verified.frameSizeSamples = context->frame_size;
    verified.delaySamples = context->delay;
    if (verified.frameSizeSamples <= 0 || verified.delaySamples < 0) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported(
                "selected audio encoder does not report bounded frame and delay facts"));
    }
    const std::int64_t effectiveMaximum = context->rc_max_rate > 0
        ? context->rc_max_rate : context->bit_rate;
    if (context->bit_rate <= 0 || effectiveMaximum <= 0 ||
        context->sample_rate <= 0) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::notInitialized(
                "opened audio encoder lacks effective rate and cadence readback"));
    }
    const auto peakBytes = ceilBytes(effectiveMaximum);
    const auto frameSamples = static_cast<std::uint64_t>(context->frame_size);
    const auto sampleRate = static_cast<std::uint64_t>(context->sample_rate);
    if (peakBytes > (std::numeric_limits<std::uint64_t>::max)() /
            frameSamples) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio access-unit readback is not representable"));
    }
    const auto frameNumerator = peakBytes * frameSamples;
    std::uint64_t maximumAccessUnit = frameNumerator / sampleRate +
        (frameNumerator % sampleRate != 0 ? 1U : 0U);
    if (context->codec_id == AV_CODEC_ID_AAC) {
        constexpr std::uint64_t AacFrameLengthMaximum = 8191;
        maximumAccessUnit = (std::max)(
            maximumAccessUnit, AacFrameLengthMaximum);
    }
    const std::uint64_t burst = context->rc_buffer_size > 0
        ? ceilBytes(context->rc_buffer_size)
        : maximumAccessUnit;
    verified.preparedEmission = MediaPreparedAudioEncoderEmissionEnvelope{
        ceilBytes(context->bit_rate), peakBytes, maximumAccessUnit, burst,
        sampleRate, frameSamples, 1, context->frame_size,
        context->codec_id == AV_CODEC_ID_AAC
            ? "opened-audio-context+aac-frame-length"
            : "opened-audio-context+codec-frame",
        encoder.name};
    verified.supportedSampleRates.push_back(target.sampleRate());
    if (target.profile().knowledge() == MediaAudioProfileKnowledge::Known) {
        verified.supportedProfileIds.push_back(target.profile().ffmpegProfileId());
    }
    return ::media::Result<MediaSelectedAudioEncoder>::success(std::move(verified));
}

} // namespace

::media::Result<MediaSelectedAudioEncoder> MediaAudioEncoderCapabilityProvider::verify(
    const MediaResolvedAudioTargetDecision& target)
{
    if (target.branchMode() != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio encoder capability verification requires transcode target"));
    }
    const auto candidates = encoderCandidates(target.codecName());
    if (candidates.empty()) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported(
                "audio encoder capability not found for codec: " + target.codecName()));
    }
    std::optional<::media::ErrorInfo> lastError;
    for (const AVCodec* candidate : candidates) {
        auto verified = verifyCandidate(*candidate, target);
        if (verified) return verified;
        lastError = verified.error();
    }
    return ::media::Result<MediaSelectedAudioEncoder>::failure(
        lastError ? *lastError
                  : ::media::ErrorInfo::unsupported(
                        "audio encoder capability verification found no executable candidate"));
}

} // namespace media::ffmpeg::graph
