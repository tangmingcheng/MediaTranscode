#include "internal/FFmpegHardwareDecoder.h"

#include "internal/FFmpegHardwareContext.h"

#include <array>

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

    HardwareDeviceType mapDeviceType(AVHWDeviceType candidate)
    {
        return HardwareDeviceContext::fromAVDeviceType(candidate);
    }

    HardwareDecoderSupport::Config findConfigByDeviceType(const AVCodec* decoder,
                                                          HardwareDeviceType requestedDeviceType)
    {
        HardwareDecoderSupport::Config result;

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

            const HardwareDeviceType mappedDeviceType = mapDeviceType(config->device_type);
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

    const std::array<HardwareDeviceType, 6>& autoDevicePriority()
    {
#if defined(_WIN32)
        static const std::array<HardwareDeviceType, 6> kPriority = {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::QSV,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::DRM,
            HardwareDeviceType::VideoToolbox
        };
#elif defined(__APPLE__)
        static const std::array<HardwareDeviceType, 6> kPriority = {
            HardwareDeviceType::VideoToolbox,
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA
        };
#else
        static const std::array<HardwareDeviceType, 6> kPriority = {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VideoToolbox
        };
#endif
        return kPriority;
    }

} // namespace

    HardwareDecoderSupport::Config HardwareDecoderSupport::findConfig(
        const AVCodec* decoder,
        HardwareDeviceType requestedDeviceType)
    {
        if (requestedDeviceType != HardwareDeviceType::Auto) {
            return findConfigByDeviceType(decoder, requestedDeviceType);
        }

        for (HardwareDeviceType candidateDeviceType : autoDevicePriority()) {
            HardwareDecoderSupport::Config config = findConfigByDeviceType(
                decoder,
                candidateDeviceType
            );

            if (config.valid) {
                return config;
            }
        }

        return HardwareDecoderSupport::Config{};
    }

    bool HardwareDecoderSupport::hasHardwareConfig(const AVCodec* decoder,
                                                   HardwareDeviceType requestedDeviceType)
    {
        return findConfig(decoder, requestedDeviceType).valid;
    }

} // namespace media::ffmpeg
