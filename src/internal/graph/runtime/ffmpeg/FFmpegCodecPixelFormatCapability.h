#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

struct FFmpegCodecPixelFormatRequirement {
    AVHWDeviceType deviceType = AV_HWDEVICE_TYPE_NONE;
    bool requireDecoderMethod = false;
    bool requireHardwareDeviceContext = false;
};

inline bool ffmpegCodecSupportsPixelFormat(
    const AVCodec* codec,
    AVPixelFormat pixelFormat,
    FFmpegCodecPixelFormatRequirement requirement = {}) noexcept
{
    if (!codec || pixelFormat == AV_PIX_FMT_NONE) {
        return false;
    }

    for (int index = 0;; ++index) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, index);
        if (!config) {
            break;
        }
        if (config->pix_fmt != pixelFormat) {
            continue;
        }
        if (!requirement.requireDecoderMethod) {
            return true;
        }
        if (requirement.requireHardwareDeviceContext) {
            const bool supportsDeviceContext =
                (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
            if (supportsDeviceContext && config->device_type == requirement.deviceType) {
                return true;
            }
        } else if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) != 0) {
            return true;
        }
    }

    if (!requirement.requireDecoderMethod) {
        for (const AVPixelFormat* format = codec->pix_fmts;
             format && *format != AV_PIX_FMT_NONE; ++format) {
            if (*format == pixelFormat) {
                return true;
            }
        }
    }
    return false;
}

} // namespace media::ffmpeg::graph
