#include "internal/graph/builder/codec/CodecResolverEncoderFormatPlanner.h"

#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string("unknown");
}

::media::Result<AVPixelFormat> parseRequiredPixelFormat(const MediaNodeOptions* options,
                                                        const std::string& key,
                                                        const std::string& owner)
{
    const std::string value = optionValue(options, key);
    if (value.empty() || value == "auto" || value == "source" || value == "inherit") {
        return ::media::Result<AVPixelFormat>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " requires explicit " + key));
    }

    const AVPixelFormat format = av_get_pix_fmt(value.c_str());
    if (format == AV_PIX_FMT_NONE) {
        return ::media::Result<AVPixelFormat>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " has invalid pixel format in " + key + ": " + value));
    }

    return ::media::Result<AVPixelFormat>::success(format);
}

::media::Result<AVPixelFormat> parseOptionalPixelFormat(const MediaNodeOptions* options,
                                                        const std::string& key,
                                                        const std::string& owner)
{
    const std::string value = optionValue(options, key);
    if (value.empty() || value == "none") {
        return ::media::Result<AVPixelFormat>::success(AV_PIX_FMT_NONE);
    }

    if (value == "auto" || value == "source" || value == "inherit") {
        return ::media::Result<AVPixelFormat>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " requires explicit " + key + " or empty/none"));
    }

    const AVPixelFormat format = av_get_pix_fmt(value.c_str());
    if (format == AV_PIX_FMT_NONE) {
        return ::media::Result<AVPixelFormat>::failure(
            ::media::ErrorInfo::invalidArgument(owner + " has invalid pixel format in " + key + ": " + value));
    }

    return ::media::Result<AVPixelFormat>::success(format);
}

bool codecSupportsPixelFormat(const AVCodec* codec, AVPixelFormat format)
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

::media::Status validateRequest(const CodecResolverEncoderFormatPlanRequest& request)
{
    if (!request.encoder) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner requires encoder"));
    }

    if (!request.options) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner requires planner options"));
    }

    return ::media::Status::success();
}

} // namespace

::media::Result<CodecResolverEncoderFormatPlan> CodecResolverEncoderFormatPlanner::build(
    const CodecResolverEncoderFormatPlanRequest& request)
{
    auto validation = validateRequest(request);
    if (!validation) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(validation.error());
    }

    CodecResolverEncoderFormatPlan plan;

    auto encoderPixelFormat = parseRequiredPixelFormat(request.options,
                                                       "encoder.pixel_format",
                                                       "CodecResolverEncoderFormatPlanner");
    if (!encoderPixelFormat) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(encoderPixelFormat.error());
    }
    plan.encoderPixelFormat = encoderPixelFormat.value();

    if (!codecSupportsPixelFormat(request.encoder, plan.encoderPixelFormat)) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(
            ::media::ErrorInfo::unsupported("CodecResolverEncoderFormatPlanner planned encoder pixel format unsupported by " +
                                           std::string(request.encoder->name ? request.encoder->name : "encoder") +
                                           ": " + pixelFormatName(plan.encoderPixelFormat)));
    }

    auto hardwareFramesFormat = parseOptionalPixelFormat(request.options,
                                                        "encoder.hw_frames_format",
                                                        "CodecResolverEncoderFormatPlanner");
    if (!hardwareFramesFormat) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(hardwareFramesFormat.error());
    }
    plan.hardwareFramesFormat = hardwareFramesFormat.value();

    auto surfaceSoftwareFormat = parseOptionalPixelFormat(request.options,
                                                         "encoder.surface_pixel_format",
                                                         "CodecResolverEncoderFormatPlanner");
    if (!surfaceSoftwareFormat) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(surfaceSoftwareFormat.error());
    }
    plan.surfaceSoftwareFormat = surfaceSoftwareFormat.value();

    if (plan.hardwareFramesFormat != AV_PIX_FMT_NONE &&
        plan.surfaceSoftwareFormat == AV_PIX_FMT_NONE) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner requires encoder.surface_pixel_format when encoder.hw_frames_format is set"));
    }

    if (plan.hardwareFramesFormat == AV_PIX_FMT_NONE &&
        plan.surfaceSoftwareFormat != AV_PIX_FMT_NONE) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner rejects encoder.surface_pixel_format without encoder.hw_frames_format"));
    }

    return ::media::Result<CodecResolverEncoderFormatPlan>::success(plan);
}

} // namespace media::ffmpeg::graph
