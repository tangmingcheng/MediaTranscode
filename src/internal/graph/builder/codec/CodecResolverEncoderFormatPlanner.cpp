#include "internal/graph/builder/codec/CodecResolverEncoderFormatPlanner.h"

#include <string>
#include <utility>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
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

::media::Result<bool> parseRequiredBool(const MediaNodeOptions* options,
                                        const std::string& key,
                                        const std::string& owner)
{
    const std::string value = optionValue(options, key);
    if (value == "1" || value == "true") {
        return ::media::Result<bool>::success(true);
    }
    if (value == "0" || value == "false") {
        return ::media::Result<bool>::success(false);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(owner + " requires explicit boolean " + key));
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

    auto requiresDevice = parseRequiredBool(request.options,
                                            "encoder.requires_hw_device_ctx",
                                            "CodecResolverEncoderFormatPlanner");
    if (!requiresDevice) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(requiresDevice.error());
    }
    plan.requiresHardwareDeviceContext = requiresDevice.value();

    auto requiresFrames = parseRequiredBool(request.options,
                                            "encoder.requires_hw_frames_ctx",
                                            "CodecResolverEncoderFormatPlanner");
    if (!requiresFrames) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(requiresFrames.error());
    }
    plan.requiresHardwareFramesContext = requiresFrames.value();

    if (plan.requiresHardwareFramesContext &&
        (plan.hardwareFramesFormat == AV_PIX_FMT_NONE ||
         plan.surfaceSoftwareFormat == AV_PIX_FMT_NONE ||
         !plan.requiresHardwareDeviceContext)) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner requires explicit device, frame, and surface formats for a generic hardware frames context"));
    }

    if (!plan.requiresHardwareFramesContext &&
        plan.hardwareFramesFormat != AV_PIX_FMT_NONE) {
        return ::media::Result<CodecResolverEncoderFormatPlan>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderFormatPlanner rejects a synthetic hardware frames format when the contract does not require one"));
    }

    return ::media::Result<CodecResolverEncoderFormatPlan>::success(plan);
}

} // namespace media::ffmpeg::graph
