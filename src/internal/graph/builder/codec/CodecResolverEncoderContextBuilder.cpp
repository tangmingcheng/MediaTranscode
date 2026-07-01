#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"

#include "internal/graph/builder/codec/CodecResolverEncoderFormatPlanner.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

std::optional<int> intOption(const MediaNodeOptions* options, const std::string& key)
{
    if (!options) {
        return std::nullopt;
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return std::nullopt;
    }

    return std::atoi(value.c_str());
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }

    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

::media::Result<int> bitsFromKbits(int kbits, const std::string& name)
{
    if (kbits <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(name + " must be positive"));
    }

    constexpr int kBitsPerKbit = 1000;
    if (kbits > std::numeric_limits<int>::max() / kBitsPerKbit) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(name + " is too large"));
    }

    return ::media::Result<int>::success(kbits * kBitsPerKbit);
}

::media::Result<int> defaultBufferSizeFromRate(int64_t rateBitsPerSecond, const std::string& rcMode)
{
    if (rateBitsPerSecond <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder " + rcMode + " mode requires positive bitrate for default buffer size"));
    }

    if (rateBitsPerSecond > std::numeric_limits<int>::max() / 2) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder " + rcMode + " default buffer size is too large"));
    }

    return ::media::Result<int>::success(static_cast<int>(rateBitsPerSecond * 2));
}

::media::Result<AVRational> resolveFrameRate(AVFormatContext* formatContext,
                                             AVStream* stream,
                                             const MediaNodeOptions* options)
{
    const std::optional<int> fpsNum = intOption(options, MediaTranscodeOptionKey::VideoFpsNum);
    const std::optional<int> fpsDen = intOption(options, MediaTranscodeOptionKey::VideoFpsDen);
    if (fpsNum || fpsDen) {
        if (!fpsNum || !fpsDen || *fpsNum <= 0 || *fpsDen <= 0) {
            return ::media::Result<AVRational>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid video fps numerator/denominator"));
        }
        return ::media::Result<AVRational>::success(AVRational{ *fpsNum, *fpsDen });
    }

    AVRational frameRate = av_guess_frame_rate(formatContext, stream, nullptr);
    if (frameRate.num > 0 && frameRate.den > 0) {
        return ::media::Result<AVRational>::success(frameRate);
    }

    if (stream && stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
        return ::media::Result<AVRational>::success(stream->avg_frame_rate);
    }

    if (stream && stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
        return ::media::Result<AVRational>::success(stream->r_frame_rate);
    }

    return ::media::Result<AVRational>::failure(
        ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cannot resolve input frame rate; specify fps explicitly"));
}

::media::Status validateRequest(const CodecResolverEncoderContextBuildRequest& request)
{
    if (!request.formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires format context"));
    }

    if (!request.stream || !request.stream->codecpar) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires source stream"));
    }

    return ::media::Status::success();
}

::media::Status requireBitrate(const AVCodecContext* encoderContext, const std::string& rcMode)
{
    if (!encoderContext || encoderContext->bit_rate <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder " + rcMode + " mode requires video bitrate"));
    }
    return ::media::Status::success();
}

::media::Status applyQualityByRateControlMode(AVCodecContext* encoderContext,
                                              const std::string& rcMode,
                                              const std::optional<int>& quality)
{
    if (!quality) {
        return ::media::Status::success();
    }

    if (*quality < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative quality"));
    }

    const std::string value = std::to_string(*quality);
    if (rcMode == "crf") {
        setPrivateOption(encoderContext, "crf", value);
        return ::media::Status::success();
    }

    if (rcMode == "auto") {
        setPrivateOption(encoderContext, "quality", value);
        setPrivateOption(encoderContext, "q", value);
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects quality without crf/auto rate control"));
}

} // namespace

::media::Result<CodecResolverEncoderContextBuildResult> CodecResolverEncoderContextBuilder::build(
    const CodecResolverEncoderContextBuildRequest& request)
{
    auto validation = validateRequest(request);
    if (!validation) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(validation.error());
    }

    const MediaNodeOptions* options = request.options;
    AVStream* stream = request.stream;
    AVCodecParameters* params = stream->codecpar;

    const std::string plannedEncoder = optionValue(options, MediaTranscodeOptionKey::PlannedEncoder);
    if (plannedEncoder.empty() || plannedEncoder == "auto") {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires planner-selected encoder"));
    }

    const AVCodec* encoder = avcodec_find_encoder_by_name(plannedEncoder.c_str());
    if (!encoder) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::unsupported("CodecResolverEncoderContextBuilder encoder not found: " + plannedEncoder));
    }

    auto formatPlanResult = CodecResolverEncoderFormatPlanner::build(
        CodecResolverEncoderFormatPlanRequest{ encoder, options });
    if (!formatPlanResult) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(formatPlanResult.error());
    }
    const CodecResolverEncoderFormatPlan formatPlan = formatPlanResult.value();

    auto frameRateResult = resolveFrameRate(request.formatContext, stream, options);
    if (!frameRateResult) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(frameRateResult.error());
    }
    const AVRational frameRate = std::move(frameRateResult).value();

    const int targetWidth = intOption(options, MediaTranscodeOptionKey::VideoWidth).value_or(params->width);
    const int targetHeight = intOption(options, MediaTranscodeOptionKey::VideoHeight).value_or(params->height);
    if (targetWidth <= 0 || targetHeight <= 0) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid target dimensions"));
    }

    CodecResolverEncoderContextBuildResult result;
    result.hardwareFramesFormat = formatPlan.hardwareFramesFormat;
    result.surfaceSoftwareFormat = formatPlan.surfaceSoftwareFormat;

    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverEncoderContextBuilder failed: avcodec_alloc_context3 returned null"));
    }

    encoderContext->width = targetWidth;
    encoderContext->height = targetHeight;
    encoderContext->pix_fmt = formatPlan.encoderPixelFormat;
    encoderContext->time_base = AVRational{ frameRate.den, frameRate.num };
    encoderContext->framerate = frameRate;
    encoderContext->sample_aspect_ratio = stream->sample_aspect_ratio;
    encoderContext->color_range = params->color_range;
    encoderContext->color_primaries = params->color_primaries;
    encoderContext->color_trc = params->color_trc;
    encoderContext->colorspace = params->color_space;

    if (auto bitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoBitrateKbps)) {
        if (*bitrateKbps < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video bitrate"));
        }
        if (*bitrateKbps > 0) {
            encoderContext->bit_rate = static_cast<int64_t>(*bitrateKbps) * 1000;
        }
    } else if (params->bit_rate > 0) {
        encoderContext->bit_rate = params->bit_rate;
    }

    if (auto minBitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoMinBitrateKbps)) {
        if (*minBitrateKbps < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video min bitrate"));
        }
        encoderContext->rc_min_rate = static_cast<int64_t>(*minBitrateKbps) * 1000;
    }

    if (auto maxBitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoMaxBitrateKbps)) {
        if (*maxBitrateKbps < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video max bitrate"));
        }
        encoderContext->rc_max_rate = static_cast<int64_t>(*maxBitrateKbps) * 1000;
    }

    if (encoderContext->rc_min_rate > 0 && encoderContext->rc_max_rate > 0 && encoderContext->rc_min_rate > encoderContext->rc_max_rate) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires video min bitrate <= max bitrate"));
    }

    if (auto bufferSizeKbits = intOption(options, MediaTranscodeOptionKey::VideoBufferSizeKbits)) {
        auto bufferSizeBits = bitsFromKbits(*bufferSizeKbits, "video buffer size");
        if (!bufferSizeBits) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bufferSizeBits.error());
        }
        encoderContext->rc_buffer_size = bufferSizeBits.value();
    }

    if (auto gop = intOption(options, MediaTranscodeOptionKey::VideoGop)) {
        if (*gop < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative gop"));
        }
        encoderContext->gop_size = *gop;
    }

    if (auto bframes = intOption(options, MediaTranscodeOptionKey::VideoBFrames)) {
        if (*bframes < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative bframes"));
        }
        encoderContext->max_b_frames = *bframes;
    }

    if (result.hardwareFramesFormat != AV_PIX_FMT_NONE) {
        auto framesStatus = configureEncoderHardwareFrames(encoderContext.get(),
                                                           request.hardwareDevice,
                                                           result.hardwareFramesFormat,
                                                           result.surfaceSoftwareFormat,
                                                           targetWidth,
                                                           targetHeight,
                                                           32);
        if (!framesStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(framesStatus.error());
        }
    }

    const std::string rcMode = optionValue(options, MediaTranscodeOptionKey::VideoRateControl);
    if (rcMode == "cbr") {
        auto bitrateStatus = requireBitrate(encoderContext.get(), rcMode);
        if (!bitrateStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bitrateStatus.error());
        }
        if (encoderContext->rc_min_rate <= 0) {
            encoderContext->rc_min_rate = encoderContext->bit_rate;
        }
        if (encoderContext->rc_max_rate <= 0) {
            encoderContext->rc_max_rate = encoderContext->bit_rate;
        }
        if (encoderContext->rc_buffer_size <= 0) {
            auto defaultBufferSize = defaultBufferSizeFromRate(encoderContext->bit_rate, rcMode);
            if (!defaultBufferSize) {
                return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(defaultBufferSize.error());
            }
            encoderContext->rc_buffer_size = defaultBufferSize.value();
        }
    } else if (rcMode == "cvbr") {
        auto bitrateStatus = requireBitrate(encoderContext.get(), rcMode);
        if (!bitrateStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bitrateStatus.error());
        }
        if (encoderContext->rc_max_rate <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cvbr mode requires video max bitrate"));
        }
        if (encoderContext->rc_buffer_size <= 0) {
            auto defaultBufferSize = defaultBufferSizeFromRate(encoderContext->rc_max_rate, rcMode);
            if (!defaultBufferSize) {
                return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(defaultBufferSize.error());
            }
            encoderContext->rc_buffer_size = defaultBufferSize.value();
        }
    } else if (rcMode == "vbr" && encoderContext->bit_rate > 0 && encoderContext->rc_buffer_size <= 0) {
        auto defaultBufferSize = defaultBufferSizeFromRate(encoderContext->bit_rate, rcMode);
        if (!defaultBufferSize) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(defaultBufferSize.error());
        }
        encoderContext->rc_buffer_size = defaultBufferSize.value();
    }

    setPrivateOption(encoderContext.get(), "preset", optionValue(options, MediaTranscodeOptionKey::VideoPreset));
    setPrivateOption(encoderContext.get(), "profile", optionValue(options, MediaTranscodeOptionKey::VideoProfile));
    setPrivateOption(encoderContext.get(), "tune", optionValue(options, MediaTranscodeOptionKey::VideoTune));
    setPrivateOption(encoderContext.get(), "level", optionValue(options, MediaTranscodeOptionKey::VideoLevel));

    auto qualityStatus = applyQualityByRateControlMode(encoderContext.get(),
                                                       rcMode,
                                                       intOption(options, MediaTranscodeOptionKey::VideoQuality));
    if (!qualityStatus) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(qualityStatus.error());
    }

    const int openRet = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (openRet < 0) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            FFmpegGraphError::statusFromCode(openRet, "avcodec_open2(video encoder " + plannedEncoder + ")").error());
    }

    result.context = std::move(encoderContext);
    return ::media::Result<CodecResolverEncoderContextBuildResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
