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
                "scale_d3d11",
                true,
                false
            };

        case HardwareDeviceType::CUDA:
            return HardwareBackendProfile{
                HardwareDeviceType::CUDA,
                "cuda",
                AV_PIX_FMT_CUDA,
                "scale_cuda",
                true,
                false
            };

        case HardwareDeviceType::QSV:
            return HardwareBackendProfile{
                HardwareDeviceType::QSV,
                "qsv",
                AV_PIX_FMT_QSV,
                "scale_qsv",
                true,
                false
            };

        case HardwareDeviceType::VAAPI:
            return HardwareBackendProfile{
                HardwareDeviceType::VAAPI,
                "vaapi",
                AV_PIX_FMT_VAAPI,
                "scale_vaapi",
                true,
                false
            };

        case HardwareDeviceType::DRM:
            return HardwareBackendProfile{
                HardwareDeviceType::DRM,
                "drm",
                AV_PIX_FMT_DRM_PRIME,
                nullptr,
                false,
                false
            };

        case HardwareDeviceType::RKMPP:
            return HardwareBackendProfile{
                HardwareDeviceType::RKMPP,
                "rkmpp",
                AV_PIX_FMT_DRM_PRIME,
                nullptr,
                false,
                true
            };

        case HardwareDeviceType::VideoToolbox:
            return HardwareBackendProfile{
                HardwareDeviceType::VideoToolbox,
                "videotoolbox",
                AV_PIX_FMT_VIDEOTOOLBOX,
                nullptr,
                false,
                false
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

} // namespace media::ffmpeg
