#include "internal/FFmpegHardwareEncoderSelector.h"

#include "internal/FFmpegUtils.h"

#include <array>
#include <vector>

extern "C" {
#include <libavcodec/version_major.h>
}

namespace media::ffmpeg {
namespace {

    using EncoderNameList = std::array<const char*, 5>;

    const AVPixelFormat kSoftwarePixelFormatSentinel = AV_PIX_FMT_NONE;

    EncoderNameList encoderCandidates(VideoCodec codec, HardwareDeviceType deviceType)
    {
        switch (deviceType) {
        case HardwareDeviceType::D3D11VA:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_mf", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_mf", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::CUDA:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_nvenc", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_nvenc", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::AV1:
                return { "av1_nvenc", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::QSV:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_qsv", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_qsv", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::AV1:
                return { "av1_qsv", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::VAAPI:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_vaapi", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_vaapi", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::DRM:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_rkmpp", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_rkmpp", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::VideoToolbox:
            switch (codec) {
            case VideoCodec::H264:
                return { "h264_videotoolbox", nullptr, nullptr, nullptr, nullptr };
            case VideoCodec::H265:
                return { "hevc_videotoolbox", nullptr, nullptr, nullptr, nullptr };
            default:
                return { nullptr, nullptr, nullptr, nullptr, nullptr };
            }

        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            return { nullptr, nullptr, nullptr, nullptr, nullptr };
        }
    }

#if LIBAVCODEC_VERSION_MAJOR >= 61
    const AVPixelFormat* supportedPixelFormats(const AVCodec* encoder, int* count)
    {
        if (count) {
            *count = 0;
        }

        if (!encoder) {
            return nullptr;
        }

        const void* configs = nullptr;
        int configCount = 0;
        const int ret = avcodec_get_supported_config(
            nullptr,
            encoder,
            AV_CODEC_CONFIG_PIX_FORMAT,
            0,
            &configs,
            &configCount
        );

        if (ret < 0 || !configs || configCount <= 0) {
            return nullptr;
        }

        if (count) {
            *count = configCount;
        }

        return static_cast<const AVPixelFormat*>(configs);
    }
#endif

    bool pixelFormatListContains(const AVPixelFormat* formats,
                                 int count,
                                 AVPixelFormat target)
    {
        if (!formats || target == AV_PIX_FMT_NONE) {
            return false;
        }

        if (count >= 0) {
            for (int i = 0; i < count; ++i) {
                if (formats[i] == target) {
                    return true;
                }
            }
            return false;
        }

        for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == target) {
                return true;
            }
        }

        return false;
    }

    AVPixelFormat firstSupportedPreferredFormat(const AVCodec* encoder,
                                                const std::vector<AVPixelFormat>& preferred)
    {
        if (!encoder) {
            return AV_PIX_FMT_NONE;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &formatCount);
        if (formats && formatCount > 0) {
            for (AVPixelFormat format : preferred) {
                if (pixelFormatListContains(formats, formatCount, format)) {
                    return format;
                }
            }
            return formats[0];
        }
#else
        if (encoder->pix_fmts) {
            for (AVPixelFormat format : preferred) {
                if (pixelFormatListContains(encoder->pix_fmts, -1, format)) {
                    return format;
                }
            }
            return encoder->pix_fmts[0];
        }
#endif

        return preferred.empty() ? AV_PIX_FMT_NONE : preferred.front();
    }

} // namespace

    HardwareEncoderSelection HardwareEncoderSelector::select(VideoCodec codec,
                                                             const HardwareBackendProfile& backend,
                                                             bool preferZeroCopy)
    {
        HardwareEncoderSelection selection;
        selection.backend = backend;

        const char* encoderName = firstAvailableEncoder(codec, backend.deviceType);
        if (!encoderName) {
            return selection;
        }

        const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName);
        if (!encoder) {
            return selection;
        }

        const bool canZeroCopy = preferZeroCopy &&
            backend.supportsZeroCopyFilter &&
            backend.hardwarePixelFormat != AV_PIX_FMT_NONE &&
            encoderSupportsPixelFormat(encoder, backend.hardwarePixelFormat);

        selection.encoder = encoder;
        selection.encoderName = encoderName;
        selection.hardwareEncoder = isHardwareEncoderName(encoderName);
        selection.zeroCopy = canZeroCopy;
        selection.pixelFormat = canZeroCopy
            ? backend.hardwarePixelFormat
            : chooseFallbackSoftwarePixelFormat(encoder, backend.deviceType);

        return selection;
    }

    bool HardwareEncoderSelector::encoderSupportsPixelFormat(const AVCodec* encoder,
                                                            AVPixelFormat pixelFormat)
    {
        if (!encoder || pixelFormat == AV_PIX_FMT_NONE) {
            return false;
        }

#if LIBAVCODEC_VERSION_MAJOR >= 61
        int formatCount = 0;
        const AVPixelFormat* formats = supportedPixelFormats(encoder, &formatCount);
        if (!formats || formatCount <= 0) {
            return false;
        }

        return pixelFormatListContains(formats, formatCount, pixelFormat);
#else
        if (!encoder->pix_fmts) {
            return false;
        }

        return pixelFormatListContains(encoder->pix_fmts, -1, pixelFormat);
#endif
    }

    AVPixelFormat HardwareEncoderSelector::chooseFallbackSoftwarePixelFormat(
        const AVCodec* encoder,
        HardwareDeviceType deviceType)
    {
        std::vector<AVPixelFormat> preferred;

        switch (deviceType) {
        case HardwareDeviceType::D3D11VA:
        case HardwareDeviceType::DRM:
        case HardwareDeviceType::VideoToolbox:
            preferred = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
            break;

        case HardwareDeviceType::CUDA:
        case HardwareDeviceType::QSV:
        case HardwareDeviceType::VAAPI:
            preferred = { AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P };
            break;

        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            preferred = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NV12 };
            break;
        }

        AVPixelFormat selected = firstSupportedPreferredFormat(encoder, preferred);
        if (selected == kSoftwarePixelFormatSentinel) {
            selected = chooseVideoEncoderPixelFormat(encoder);
        }

        return selected;
    }

    const char* HardwareEncoderSelector::firstAvailableEncoder(VideoCodec codec,
                                                               HardwareDeviceType deviceType)
    {
        const EncoderNameList names = encoderCandidates(codec, deviceType);
        for (const char* name : names) {
            if (!name) {
                continue;
            }

            if (avcodec_find_encoder_by_name(name)) {
                return name;
            }
        }

        return nullptr;
    }

} // namespace media::ffmpeg
