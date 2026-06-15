#pragma once

#include "internal/FFmpegHardwareBackend.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    struct HardwareEncoderSelection {
        const AVCodec* encoder = nullptr;
        std::string encoderName;
        AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
        bool zeroCopy = false;
        bool hardwareEncoder = false;
        HardwareBackendProfile backend;
    };

    class HardwareEncoderSelector {
    public:
        static HardwareEncoderSelection select(VideoCodec codec,
                                               const HardwareBackendProfile& backend,
                                               bool preferZeroCopy);

        static bool encoderSupportsPixelFormat(const AVCodec* encoder,
                                               AVPixelFormat pixelFormat);

        static AVPixelFormat chooseFallbackSoftwarePixelFormat(const AVCodec* encoder,
                                                               HardwareDeviceType deviceType);

    private:
        static const char* firstAvailableEncoder(VideoCodec codec,
                                                 HardwareDeviceType deviceType);
    };

} // namespace media::ffmpeg
