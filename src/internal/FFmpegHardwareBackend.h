#pragma once

#include "internal/FFmpegHardwareTypes.h"

#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    struct HardwareBackendProfile {
        HardwareDeviceType deviceType = HardwareDeviceType::None;
        const char* name = "none";
        AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;
        const char* scaleFilterName = nullptr;
        bool supportsZeroCopyFilter = false;
        bool supportsZeroCopyFrameRateFilter = true;
        bool supportsDirectHardwareFrameEncode = false;
        bool encoderRequiresHardwareDeviceContext = true;
        AVPixelFormat directHardwareFrameSoftwareFormat = AV_PIX_FMT_NONE;

        // Preferred software layout inside hardware surfaces when a hardware
        // encoder consumes AV_PIX_FMT_* hardware frames. For NVIDIA CUDA/NVENC
        // this is normally NV12 for H.264/HEVC 8-bit zero-copy paths.
        AVPixelFormat preferredHardwareFrameSoftwareFormat = AV_PIX_FMT_NONE;

        // Hardware scale filters do not all expose the same option surface. Keep
        // the syntax detail in the backend profile so the generic planner does
        // not need backend-specific if/else branches for common format output.
        bool scaleFilterSupportsFormatOption = false;
        const char* scaleFilterFormatOptionName = "format";
    };

    class HardwareBackendRegistry {
    public:
        static HardwareBackendProfile profileFor(HardwareDeviceType deviceType);
        static AVPixelFormat hardwarePixelFormat(HardwareDeviceType deviceType);
        static const char* scaleFilterName(HardwareDeviceType deviceType);
        static bool supportsZeroCopyFilter(HardwareDeviceType deviceType);
        static bool supportsDirectHardwareFrameEncode(HardwareDeviceType deviceType);

        static std::vector<HardwareDeviceType> backendPriority(HardwareDeviceType requestedDeviceType);
    };

} // namespace media::ffmpeg
