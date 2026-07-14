#include "internal/graph/planner/audio/capability/MediaAudioEncoderCapabilityProvider.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
}

#include <string>
#include <optional>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

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
    const bool profileChanged =
        target.profile().knowledge() == MediaAudioProfileKnowledge::Known &&
        context->profile != target.profile().ffmpegProfileId();
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const bool channelsChanged = context->ch_layout.nb_channels != target.channels();
#else
    const bool channelsChanged = context->channels != target.channels();
#endif
    if (context->sample_rate != target.sampleRate() ||
        context->sample_fmt != sampleFormat || channelsChanged || profileChanged) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported(
                "audio encoder changed the exact planner target during capability verification"));
    }

    MediaSelectedAudioEncoder verified;
    verified.name = encoder.name;
    verified.sampleFormat = sampleFormatName;
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
