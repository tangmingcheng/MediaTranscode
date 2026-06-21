#pragma once

#include "internal/FFmpegHardwareBackend.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    struct HardwareEncoderCandidate {
        std::string encoderName;
        bool available = false;
        bool hardwareEncoder = false;
        bool supportsBackendHardwareFrames = false;
        std::string pixelFormats;
        std::string rejectionReason;
    };

    struct HardwareEncoderSelection {
        const AVCodec* encoder = nullptr;
        std::string encoderName;
        AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
        bool zeroCopy = false;
        bool hardwareEncoder = false;
        HardwareBackendProfile backend;
        std::vector<HardwareEncoderCandidate> candidates;
        std::string diagnostic;
    };

    class HardwareEncoderSelector {
    public:
        static HardwareEncoderSelection selectZeroCopyEncoder(VideoCodec codec,
                                                              const HardwareBackendProfile& backend);

        // Compatibility wrapper. Prefer selectHardwareEncoderForSoftwareFrames().
        static HardwareEncoderSelection selectMixedGpuEncoder(VideoCodec codec,
                                                              const HardwareBackendProfile& backend);

        static HardwareEncoderSelection selectHardwareEncoderForSoftwareFrames(
            VideoCodec codec,
            const HardwareBackendProfile& backend)
        {
            return selectMixedGpuEncoder(codec, backend);
        }

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
