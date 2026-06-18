#include "internal/FFmpegHardwareDecoder.h"

#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegHardwareContext.h"

#include <string>

namespace media::ffmpeg {
namespace {

    bool isRkmppDevice(HardwareDeviceType requestedDeviceType)
    {
        return requestedDeviceType == HardwareDeviceType::RKMPP;
    }

    bool isCudaDevice(HardwareDeviceType requestedDeviceType)
    {
        return requestedDeviceType == HardwareDeviceType::CUDA;
    }

    bool matchesRequestedDevice(const AVCodecHWConfig* config,
                                HardwareDeviceType requestedDeviceType)
    {
        if (!config) {
            return false;
        }

        if (isRkmppDevice(requestedDeviceType)) {
            return config->pix_fmt == AV_PIX_FMT_DRM_PRIME &&
                ((config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) ||
                 (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX));
        }

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            return false;
        }

        if (requestedDeviceType == HardwareDeviceType::Auto) {
            return true;
        }

        return config->device_type == HardwareDeviceContext::toAVDeviceType(requestedDeviceType);
    }

    HardwareDeviceType mapDeviceType(const AVCodecHWConfig* config,
                                     HardwareDeviceType requestedDeviceType)
    {
        if (requestedDeviceType != HardwareDeviceType::Auto &&
            requestedDeviceType != HardwareDeviceType::None) {
            return requestedDeviceType;
        }

        return config ? HardwareDeviceContext::fromAVDeviceType(config->device_type) : HardwareDeviceType::None;
    }

    AVHWDeviceType mapAVDeviceType(const AVCodecHWConfig* config,
                                   HardwareDeviceType requestedDeviceType)
    {
        if (isRkmppDevice(requestedDeviceType)) {
            return AV_HWDEVICE_TYPE_DRM;
        }

        return config ? config->device_type : AV_HWDEVICE_TYPE_NONE;
    }

    bool requiresHardwareDeviceContext(const AVCodecHWConfig* config,
                                       HardwareDeviceType requestedDeviceType)
    {
        if (!config) {
            return false;
        }

        if (isRkmppDevice(requestedDeviceType) &&
            (config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL)) {
            return false;
        }

        return (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
    }

    const char* rkmppDecoderName(AVCodecID codecId)
    {
        switch (codecId) {
        case AV_CODEC_ID_H264:
            return "h264_rkmpp";
        case AV_CODEC_ID_HEVC:
            return "hevc_rkmpp";
        case AV_CODEC_ID_VP8:
            return "vp8_rkmpp";
        case AV_CODEC_ID_VP9:
            return "vp9_rkmpp";
        default:
            return nullptr;
        }
    }

    const char* cudaDecoderName(AVCodecID codecId)
    {
        switch (codecId) {
        case AV_CODEC_ID_H264:
            return "h264_cuvid";
        case AV_CODEC_ID_HEVC:
            return "hevc_cuvid";
        case AV_CODEC_ID_AV1:
            return "av1_cuvid";
        case AV_CODEC_ID_VP8:
            return "vp8_cuvid";
        case AV_CODEC_ID_VP9:
            return "vp9_cuvid";
        case AV_CODEC_ID_MPEG2VIDEO:
            return "mpeg2_cuvid";
        case AV_CODEC_ID_MPEG4:
            return "mpeg4_cuvid";
        case AV_CODEC_ID_MJPEG:
            return "mjpeg_cuvid";
        default:
            return nullptr;
        }
    }

    const AVCodec* decoderForDevice(const AVCodec* defaultDecoder,
                                    HardwareDeviceType requestedDeviceType)
    {
        if (!defaultDecoder) {
            return nullptr;
        }

        if (requestedDeviceType == HardwareDeviceType::RKMPP) {
            const char* name = rkmppDecoderName(defaultDecoder->id);
            return name ? avcodec_find_decoder_by_name(name) : nullptr;
        }

        if (isCudaDevice(requestedDeviceType)) {
            const char* name = cudaDecoderName(defaultDecoder->id);
            if (name) {
                if (const AVCodec* cudaDecoder = avcodec_find_decoder_by_name(name)) {
                    return cudaDecoder;
                }
            }
        }

        return defaultDecoder;
    }

    HardwareDecoderSupport::Config findConfigByDeviceType(const AVCodec* decoder,
                                                          HardwareDeviceType requestedDeviceType)
    {
        HardwareDecoderSupport::Config result;

        if (!decoder || requestedDeviceType == HardwareDeviceType::None) {
            return result;
        }

        const AVCodec* selectedDecoder = decoderForDevice(decoder, requestedDeviceType);
        if (!selectedDecoder) {
            return result;
        }

        for (int i = 0;; ++i) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(selectedDecoder, i);
            if (!config) {
                break;
            }

            if (!matchesRequestedDevice(config, requestedDeviceType)) {
                continue;
            }

            const HardwareDeviceType mappedDeviceType = mapDeviceType(
                config,
                requestedDeviceType
            );

            if (mappedDeviceType == HardwareDeviceType::None) {
                continue;
            }

            result.valid = true;
            result.deviceType = mappedDeviceType;
            result.avDeviceType = mapAVDeviceType(config, requestedDeviceType);
            result.hardwarePixelFormat = config->pix_fmt;
            result.decoder = selectedDecoder;
            result.decoderName = selectedDecoder->name ? selectedDecoder->name : "";
            result.requiresHardwareDeviceContext = requiresHardwareDeviceContext(
                config,
                requestedDeviceType
            );
            return result;
        }

        return result;
    }

} // namespace

    HardwareDecoderSupport::Config HardwareDecoderSupport::findConfig(
        const AVCodec* decoder,
        HardwareDeviceType requestedDeviceType)
    {
        if (requestedDeviceType != HardwareDeviceType::Auto) {
            return findConfigByDeviceType(decoder, requestedDeviceType);
        }

        for (HardwareDeviceType candidateDeviceType :
             HardwareBackendRegistry::backendPriority(HardwareDeviceType::Auto)) {
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
