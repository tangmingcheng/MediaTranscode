#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

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
    };

    class HardwareBackendRegistry {
    public:
        static HardwareBackendProfile profileFor(HardwareDeviceType deviceType);
        static AVPixelFormat hardwarePixelFormat(HardwareDeviceType deviceType);
        static const char* scaleFilterName(HardwareDeviceType deviceType);
        static bool supportsZeroCopyFilter(HardwareDeviceType deviceType);
    };

} // namespace media::ffmpeg
