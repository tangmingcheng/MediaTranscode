#pragma once

#include "internal/FFmpegHardwareBackend.h"

#include <string>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    enum class HardwareVideoFilterStepType {
        FrameRate,
        HardwareScale,
        HardwareDownload,
        SoftwareFormat
    };

    struct HardwareVideoFilterStep {
        HardwareVideoFilterStepType type = HardwareVideoFilterStepType::HardwareScale;
        std::string expression;
    };

    struct HardwareVideoFilterRequest {
        HardwareBackendProfile backend;

        int outputWidth = 0;
        int outputHeight = 0;
        AVPixelFormat softwareFormat = AV_PIX_FMT_NONE;

        int outputFps = 0;
        bool enableConstantFps = false;

        bool enableScale = false;
        bool enableFormatConversion = false;
        bool keepFramesOnDevice = true;
    };

    struct HardwareVideoFilterPlan {
        std::string description;
        std::vector<HardwareVideoFilterStep> steps;

        bool hasFrameRateFilter = false;
        bool hasHardwareScale = false;
        bool downloadsToSoftware = false;
        bool hasSoftwareFormat = false;
        bool keepsFramesOnDevice = true;
    };

    class HardwareVideoFilterPipelinePlanner {
    public:
        static HardwareVideoFilterPlan build(const HardwareVideoFilterRequest& request,
                                             std::string* error = nullptr);

        static bool supportsHardwareScale(const HardwareBackendProfile& backend);
        static const char* softwarePixelFormatName(AVPixelFormat format);
    };

} // namespace media::ffmpeg
