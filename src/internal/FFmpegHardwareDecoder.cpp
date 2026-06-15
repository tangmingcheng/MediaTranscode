#include "internal/FFmpegHardwareDecoder.h"

#include "internal/FFmpegHardwareContext.h"

namespace media::ffmpeg {
namespace {

    bool matchesRequestedDevice(AVHWDeviceType candidate,
                                HardwareDeviceType requestedDeviceType)
    {
        if (requestedDeviceType == HardwareDeviceType::Auto) {
            return true;
        }

        return candidate == HardwareDeviceContext::toAVDeviceType(requestedDeviceType);
    }

} // namespace

    HardwareDecoderSupport::Config HardwareDecoderSupport::findConfig(
        const AVCodec* decoder,
        HardwareDeviceType requestedDeviceType)
    {
        Config result;

        if (!decoder || requestedDeviceType == HardwareDeviceType::None) {
            return result;
        }

        for (int i = 0;; ++i) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                break;
            }

            if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                continue;
            }

            if (!matchesRequestedDevice(config->device_type, requestedDeviceType)) {
                continue;
            }

            const HardwareDeviceType mappedDeviceType =
                HardwareDeviceContext::fromAVDeviceType(config->device_type);
            if (mappedDeviceType == HardwareDeviceType::None) {
                continue;
            }

            result.valid = true;
            result.deviceType = mappedDeviceType;
            result.avDeviceType = config->device_type;
            result.hardwarePixelFormat = config->pix_fmt;
            return result;
        }

        return result;
    }

    bool HardwareDecoderSupport::hasHardwareConfig(const AVCodec* decoder,
                                                   HardwareDeviceType requestedDeviceType)
    {
        return findConfig(decoder, requestedDeviceType).valid;
    }

} // namespace media::ffmpeg
