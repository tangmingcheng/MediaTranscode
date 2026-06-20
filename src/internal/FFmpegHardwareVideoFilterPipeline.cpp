#include "internal/FFmpegHardwareVideoFilterPipeline.h"

#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    bool hasValidBackend(const HardwareBackendProfile& backend)
    {
        return backend.deviceType != HardwareDeviceType::None &&
            backend.deviceType != HardwareDeviceType::Auto;
    }

    bool wantsFrameRateFilter(const HardwareVideoFilterRequest& request)
    {
        return request.enableConstantFps && request.outputFps > 0;
    }

    bool wantsHardwareTransform(const HardwareVideoFilterRequest& request)
    {
        return request.enableScale || request.enableFormatConversion;
    }

    void setError(std::string* error, std::string message)
    {
        if (error) {
            *error = std::move(message);
        }
    }

    void appendStep(HardwareVideoFilterPlan& plan,
                    HardwareVideoFilterStepType type,
                    std::string expression)
    {
        if (expression.empty()) {
            return;
        }

        plan.steps.push_back(HardwareVideoFilterStep{ type, std::move(expression) });
    }

    std::string joinFilterDescription(const std::vector<HardwareVideoFilterStep>& steps)
    {
        if (steps.empty()) {
            return "null";
        }

        std::ostringstream desc;
        bool first = true;
        for (const HardwareVideoFilterStep& step : steps) {
            if (!first) {
                desc << ",";
            }
            first = false;
            desc << step.expression;
        }

        return desc.str();
    }

    std::string frameRateExpression(int outputFps)
    {
        std::ostringstream desc;
        desc << "fps=fps=" << outputFps;
        return desc.str();
    }

    std::string hardwareScaleExpression(const HardwareVideoFilterRequest& request,
                                        std::string* error)
    {
        if (!HardwareVideoFilterPipelinePlanner::supportsHardwareScale(request.backend)) {
            setError(error, "hardware filter graph build failed: backend does not expose a hardware scale filter");
            return {};
        }

        if (request.outputWidth <= 0 || request.outputHeight <= 0) {
            setError(error, "hardware filter graph build failed: invalid output size");
            return {};
        }

        std::ostringstream desc;
        desc << request.backend.scaleFilterName
             << "="
             << request.outputWidth
             << ":"
             << request.outputHeight;

        const char* formatName = HardwareVideoFilterPipelinePlanner::softwarePixelFormatName(
            request.softwareFormat
        );

        if (request.backend.scaleFilterSupportsFormatOption && formatName && *formatName) {
            desc << ":"
                 << (request.backend.scaleFilterFormatOptionName
                         ? request.backend.scaleFilterFormatOptionName
                         : "format")
                 << "="
                 << formatName;
        }

        return desc.str();
    }

    std::string softwareFormatExpression(AVPixelFormat format)
    {
        const char* formatName = HardwareVideoFilterPipelinePlanner::softwarePixelFormatName(format);
        if (!formatName || !*formatName) {
            return {};
        }

        std::ostringstream desc;
        desc << "format=pix_fmts=" << formatName;
        return desc.str();
    }

} // namespace

    HardwareVideoFilterPlan HardwareVideoFilterPipelinePlanner::build(
        const HardwareVideoFilterRequest& request,
        std::string* error)
    {
        HardwareVideoFilterPlan plan;
        plan.keepsFramesOnDevice = request.keepFramesOnDevice;

        if (!hasValidBackend(request.backend)) {
            setError(error, "hardware filter graph build failed: invalid hardware backend");
            return {};
        }

        if (request.outputFps < 0) {
            setError(error, "hardware filter graph build failed: invalid output fps");
            return {};
        }

        const bool needFrameRate = wantsFrameRateFilter(request);
        const bool needHardwareTransform = wantsHardwareTransform(request);
        bool frameRateDeferredUntilSoftware = false;

        if (needFrameRate) {
            if (request.backend.supportsZeroCopyFrameRateFilter) {
                appendStep(plan, HardwareVideoFilterStepType::FrameRate,
                           frameRateExpression(request.outputFps));
                plan.hasFrameRateFilter = true;
            }
            else if (request.keepFramesOnDevice) {
                setError(error,
                         "hardware filter graph build failed: backend cannot apply fps while keeping frames on device");
                return {};
            }
            else {
                frameRateDeferredUntilSoftware = true;
            }
        }

        if (needHardwareTransform) {
            const std::string scaleExpr = hardwareScaleExpression(request, error);
            if (scaleExpr.empty()) {
                return {};
            }

            appendStep(plan, HardwareVideoFilterStepType::HardwareScale, scaleExpr);
            plan.hasHardwareScale = true;
        }

        if (!request.keepFramesOnDevice) {
            appendStep(plan, HardwareVideoFilterStepType::HardwareDownload, "hwdownload");
            plan.downloadsToSoftware = true;
            plan.keepsFramesOnDevice = false;

            if (frameRateDeferredUntilSoftware) {
                appendStep(plan, HardwareVideoFilterStepType::FrameRate,
                           frameRateExpression(request.outputFps));
                plan.hasFrameRateFilter = true;
            }

            const std::string formatExpr = softwareFormatExpression(request.softwareFormat);
            if (!formatExpr.empty()) {
                appendStep(plan, HardwareVideoFilterStepType::SoftwareFormat, formatExpr);
                plan.hasSoftwareFormat = true;
            }
        }

        plan.description = joinFilterDescription(plan.steps);
        return plan;
    }

    bool HardwareVideoFilterPipelinePlanner::supportsHardwareScale(
        const HardwareBackendProfile& backend)
    {
        return backend.scaleFilterName != nullptr && *backend.scaleFilterName != '\0';
    }

    const char* HardwareVideoFilterPipelinePlanner::softwarePixelFormatName(AVPixelFormat format)
    {
        if (format == AV_PIX_FMT_NONE) {
            return nullptr;
        }

        return av_get_pix_fmt_name(format);
    }

} // namespace media::ffmpeg
