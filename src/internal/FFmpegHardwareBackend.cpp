#include "internal/FFmpegHardwareBackend.h"

namespace media::ffmpeg {
namespace {

    HardwareBackendProfile noneProfile()
    {
        return HardwareBackendProfile{};
    }

} // namespace

    HardwareBackendProfile HardwareBackendRegistry::profileFor(HardwareDeviceType deviceType)
    {
        switch (deviceType) {
        case HardwareDeviceType::D3D11VA:
            return HardwareBackendProfile{
                HardwareDeviceType::D3D11VA,
                "d3d11va",
                AV_PIX_FMT_D3D11,
                nullptr,
                false,
                true,
                false,
                true,
                AV_PIX_FMT_NONE,
                AV_PIX_FMT_NV12
            };

        case HardwareDeviceType::CUDA:
            return HardwareBackendProfile{
                HardwareDeviceType::CUDA,
                "cuda",
                AV_PIX_FMT_CUDA,
                "scale_cuda",
                true,
                true,
                false,
                true,
                AV_PIX_FMT_NONE,
                AV_PIX_FMT_NV12,
                true
            };

        case HardwareDeviceType::QSV:
            return HardwareBackendProfile{
                HardwareDeviceType::QSV,
                "qsv",
                AV_PIX_FMT_QSV,
                "scale_qsv",
                true,
                true,
                false,
                true,
                AV_PIX_FMT_NONE,
                AV_PIX_FMT_NV12,
                true
            };

        case HardwareDeviceType::VAAPI:
            return HardwareBackendProfile{
                HardwareDeviceType::VAAPI,
                "vaapi",
                AV_PIX_FMT_VAAPI,
                "scale_vaapi",
                true,
                true,
                false,
                true,
                AV_PIX_FMT_NONE,
                AV_PIX_FMT_NV12,
                true
            };

        case HardwareDeviceType::DRM:
            return HardwareBackendProfile{
                HardwareDeviceType::DRM,
                "drm",
                AV_PIX_FMT_DRM_PRIME,
                nullptr,
                false,
                true,
                false,
                false,
                AV_PIX_FMT_NONE
            };

        case HardwareDeviceType::RKMPP:
            return HardwareBackendProfile{
                HardwareDeviceType::RKMPP,
                "rkmpp",
                AV_PIX_FMT_DRM_PRIME,
                "scale_rkrga",
                true,
                true,
                true,
                false,
                AV_PIX_FMT_NV12,
                AV_PIX_FMT_NV12
            };

        case HardwareDeviceType::VideoToolbox:
            return HardwareBackendProfile{
                HardwareDeviceType::VideoToolbox,
                "videotoolbox",
                AV_PIX_FMT_VIDEOTOOLBOX,
                nullptr,
                false,
                true,
                false,
                false,
                AV_PIX_FMT_NONE
            };

        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            return noneProfile();
        }
    }

    AVPixelFormat HardwareBackendRegistry::hardwarePixelFormat(HardwareDeviceType deviceType)
    {
        return profileFor(deviceType).hardwarePixelFormat;
    }

    const char* HardwareBackendRegistry::scaleFilterName(HardwareDeviceType deviceType)
    {
        return profileFor(deviceType).scaleFilterName;
    }

    bool HardwareBackendRegistry::supportsZeroCopyFilter(HardwareDeviceType deviceType)
    {
        return profileFor(deviceType).supportsZeroCopyFilter;
    }

    bool HardwareBackendRegistry::supportsDirectHardwareFrameEncode(HardwareDeviceType deviceType)
    {
        return profileFor(deviceType).supportsDirectHardwareFrameEncode;
    }

    std::vector<HardwareDeviceType> HardwareBackendRegistry::backendPriority(
        HardwareDeviceType requestedDeviceType)
    {
        if (requestedDeviceType != HardwareDeviceType::Auto &&
            requestedDeviceType != HardwareDeviceType::None) {
            return { requestedDeviceType };
        }

#if defined(_WIN32)
        return {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::QSV,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::RKMPP,
            HardwareDeviceType::DRM,
            HardwareDeviceType::VideoToolbox
        };
#elif defined(__APPLE__)
        return {
            HardwareDeviceType::VideoToolbox,
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::RKMPP,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA
        };
#elif defined(__aarch64__) || defined(__arm64__)
        return {
            HardwareDeviceType::RKMPP,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::DRM,
            HardwareDeviceType::CUDA,
            HardwareDeviceType::QSV,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VideoToolbox
        };
#else
        return {
            HardwareDeviceType::CUDA,
            HardwareDeviceType::VAAPI,
            HardwareDeviceType::QSV,
            HardwareDeviceType::RKMPP,
            HardwareDeviceType::DRM,
            HardwareDeviceType::D3D11VA,
            HardwareDeviceType::VideoToolbox
        };
#endif
    }

} // namespace media::ffmpeg
