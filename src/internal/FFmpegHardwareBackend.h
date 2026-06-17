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
