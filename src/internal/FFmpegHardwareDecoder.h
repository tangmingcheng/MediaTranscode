#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    /*
     * Discovers decoder-side hardware frame support.
     *
     * This module is intentionally only about decoder capability probing. Device
     * ownership stays in HardwareDeviceContext, and frame-pool ownership stays
     * in HardwareFramesContext.
     */
    class HardwareDecoderSupport {
    public:
        struct Config {
            bool valid = false;
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            AVHWDeviceType avDeviceType = AV_HWDEVICE_TYPE_NONE;
            AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;
        };

        static Config findConfig(const AVCodec* decoder,
                                 HardwareDeviceType requestedDeviceType);

        static bool hasHardwareConfig(const AVCodec* decoder,
                                      HardwareDeviceType requestedDeviceType);
    };

} // namespace media::ffmpeg
