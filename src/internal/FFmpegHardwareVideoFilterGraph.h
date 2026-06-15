#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <string>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    /*
     * Describes hardware filter graph policy only.
     *
     * This class does not own an AVFilterGraph yet. It is the explicit extension
     * point for the future hardware frame path:
     *   decoder hw frames -> hw filter -> encoder hw frames
     * rather than mixing hwupload/hwdownload/scale decisions into the current
     * software VideoFilterGraph.
     */
    class HardwareVideoFilterGraphBuilder {
    public:
        struct Config {
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            int outputWidth = 0;
            int outputHeight = 0;
            AVPixelFormat softwareFormat = AV_PIX_FMT_NONE;
            bool enableScale = false;
            bool keepFramesOnDevice = true;
        };

        static std::string buildDescription(const Config& config,
                                            std::string* error = nullptr);

        static bool supportsHardwareScale(HardwareDeviceType deviceType);
        static const char* scaleFilterName(HardwareDeviceType deviceType);
        static const char* softwarePixelFormatName(AVPixelFormat format);
    };

} // namespace media::ffmpeg
