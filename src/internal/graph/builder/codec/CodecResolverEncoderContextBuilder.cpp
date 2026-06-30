#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"

#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
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

bool pixelFormatSupported(const AVCodec* codec, AVPixelFormat format)
{
    if (!codec || !codec->pix_fmts || format == AV_PIX_FMT_NONE) {
        return true;
    }

    for (const AVPixelFormat* current = codec->pix_fmts; *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == format) {
            return true;
        }
    }

    return false;
}

AVPixelFormat plannedEncoderHardwarePixelFormat(const MediaNodeOptions* options)
{
    if (optionValue(options, "encoder.pipeline.frame_kind") != "hardware") {
        return AV_PIX_FMT_NONE;
    }

    const std::string hwaccel = optionValue(options, "encoder.pipeline.hwaccel");
    const std::string device = optionValue(options, "encoder.pipeline.device");
    if (hwaccel == "cuda" || device == "cuda") {
        return AV_PIX_FMT_CUDA;
    }
    if (hwaccel == "qsv" || device == "qsv") {
        return AV_PIX_FMT_QSV;
    }
    if (hwaccel == "vaapi" || device == "vaapi") {
        return AV_PIX_FMT_VAAPI;
    }
    if (hwaccel == "d3d11va" || device == "d3d11va") {
        return AV_PIX_FMT_D3D11;
    }

    return AV_PIX_FMT_NONE;
}

AVPixelFormat plannedEncoderSoftwarePixelFormat(const MediaNodeOptions* options, AVPixelFormat sourceFormat)
{
    const AVPixelFormat hardwareFormat = plannedEncoderHardwarePixelFormat(options);
    if (hardwareFormat == AV_PIX_FMT_CUDA &&
        (sourceFormat == AV_PIX_FMT_YUV420P || sourceFormat == AV_PIX_FMT_NONE)) {
        return AV_PIX_FMT_NV12;
    }

    if (sourceFormat != AV_PIX_FMT_NONE) {
        return sourceFormat;
    }

    if (hardwareFormat == AV_PIX_FMT_CUDA || hardwareFormat == AV_PIX_FMT_D3D11) {
        return AV_PIX_FMT_NV12;
    }

    return AV_PIX_FMT_YUV420P;
}

AVPixelFormat chooseEncoderPixelFormat(const AVCodec* encoder,
                                       AVPixelFormat sourceFormat,
                                       const MediaNodeOptions* options)
{
    const AVPixelFormat plannedHardwareFormat = plannedEncoderHardwarePixelFormat(options);
    if (plannedHardwareFormat != AV_PIX_FMT_NONE) {
        return pixelFormatSupported(encoder, plannedHardwareFormat) ? plannedHardwareFormat : AV_PIX_FMT_NONE;
    }

    if (sourceFormat != AV_PIX_FMT_NONE && pixelFormatSupported(encoder, sourceFormat)) {
        return sourceFormat;
    }

    if (encoder && encoder->pix_fmts && encoder->pix_fmts[0] != AV_PIX_FMT_NONE) {
        return encoder->pix_fmts[0];
    }

    return sourceFormat;
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }

    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

::media::Result<AVRational> resolveFrameRate(AVFormatContext* formatContext,
                                             AVStream* stream,
                                             const MediaNodeOptions* options)
{
    const std::optional<int> fpsNum = intOption(options, "fps_num");
    const std::optional<int> fpsDen = intOption(options, "fps_den");
    if (fpsNum || fpsDen) {
        if (!fpsNum || !fpsDen || *fpsNum <= 0 || *fpsDen <= 0) {
            return ::media::Result<AVRational>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid fps_num/fps_den"));
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

    const std::string plannedEncoder = optionValue(options, "encoder");
    if (plannedEncoder.empty() || plannedEncoder == "auto") {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires planner-selected encoder"));
    }

    const AVCodec* encoder = avcodec_find_encoder_by_name(plannedEncoder.c_str());
    if (!encoder) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::unsupported("CodecResolverEncoderContextBuilder encoder not found: " + plannedEncoder));
    }

    auto frameRateResult = resolveFrameRate(request.formatContext, stream, options);
    if (!frameRateResult) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(frameRateResult.error());
    }
    const AVRational frameRate = std::move(frameRateResult).value();

    const int targetWidth = intOption(options, "width").value_or(params->width);
    const int targetHeight = intOption(options, "height").value_or(params->height);
    if (targetWidth <= 0 || targetHeight <= 0) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid target dimensions"));
    }

    CodecResolverEncoderContextBuildResult result;
    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverEncoderContextBuilder failed: avcodec_alloc_context3 returned null"));
    }

    const AVPixelFormat encoderPixelFormat = chooseEncoderPixelFormat(
        encoder,
        static_cast<AVPixelFormat>(params->format),
        options);
    if (encoderPixelFormat == AV_PIX_FMT_NONE) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::unsupported("CodecResolverEncoderContextBuilder planned encoder pixel format unsupported: " + plannedEncoder));
    }

    result.hardwareFramesFormat = plannedEncoderHardwarePixelFormat(options);
    result.surfaceSoftwareFormat = plannedEncoderSoftwarePixelFormat(
        options,
        static_cast<AVPixelFormat>(params->format));

    encoderContext->width = targetWidth;
    encoderContext->height = targetHeight;
    encoderContext->pix_fmt = encoderPixelFormat;
    encoderContext->time_base = AVRational{ frameRate.den, frameRate.num };
    encoderContext->framerate = frameRate;
    encoderContext->sample_aspect_ratio = stream->sample_aspect_ratio;
    encoderContext->color_range = params->color_range;
    encoderContext->color_primaries = params->color_primaries;
    encoderContext->color_trc = params->color_trc;
    encoderContext->colorspace = params->color_space;

    if (auto bitrateKbps = intOption(options, "bitrate_kbps")) {
        if (*bitrateKbps < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative bitrate"));
        }
        if (*bitrateKbps > 0) {
            encoderContext->bit_rate = static_cast<int64_t>(*bitrateKbps) * 1000;
        }
    } else if (params->bit_rate > 0) {
        encoderContext->bit_rate = params->bit_rate;
    }

    if (auto gop = intOption(options, "gop")) {
        if (*gop < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative gop"));
        }
        encoderContext->gop_size = *gop;
    }

    if (auto bframes = intOption(options, "bframes")) {
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

    const std::string rcMode = optionValue(options, "rc");
    if (rcMode == "cbr" && encoderContext->bit_rate > 0) {
        encoderContext->rc_min_rate = encoderContext->bit_rate;
        encoderContext->rc_max_rate = encoderContext->bit_rate;
        encoderContext->rc_buffer_size = static_cast<int>(encoderContext->bit_rate * 2);
    } else if (rcMode == "vbr" && encoderContext->bit_rate > 0) {
        encoderContext->rc_max_rate = encoderContext->bit_rate;
        encoderContext->rc_buffer_size = static_cast<int>(encoderContext->bit_rate * 2);
    }

    setPrivateOption(encoderContext.get(), "preset", optionValue(options, "preset"));
    setPrivateOption(encoderContext.get(), "profile", optionValue(options, "profile"));
    setPrivateOption(encoderContext.get(), "tune", optionValue(options, "tune"));
    setPrivateOption(encoderContext.get(), "level", optionValue(options, "level"));

    if (auto crf = intOption(options, "crf")) {
        if (*crf < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative crf"));
        }
        setPrivateOption(encoderContext.get(), "crf", std::to_string(*crf));
    }

    if (auto quality = intOption(options, "quality")) {
        if (*quality < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative quality"));
        }
        setPrivateOption(encoderContext.get(), "quality", std::to_string(*quality));
        setPrivateOption(encoderContext.get(), "q", std::to_string(*quality));
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
