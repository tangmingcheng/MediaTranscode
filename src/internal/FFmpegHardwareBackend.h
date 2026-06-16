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

        // True when the backend has an FFmpeg hardware filter path that can keep
        // frames on device, for example scale_cuda/scale_vaapi/scale_qsv.
        bool supportsZeroCopyFilter = false;

        // True when the backend can pass decoder hardware frames directly to the
        // selected encoder without an intermediate hardware filter graph.
        bool supportsDirectHardwareFrameEncode = false;

        // Optional software layout carried by direct hardware frames. For example,
        // RKMPP consumes DRM_PRIME frames whose underlying MPP frame layout is NV12.
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
