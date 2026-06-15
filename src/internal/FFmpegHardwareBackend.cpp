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
                true
            };

        case HardwareDeviceType::CUDA:
            return HardwareBackendProfile{
                HardwareDeviceType::CUDA,
                "cuda",
                AV_PIX_FMT_CUDA,
                "scale_cuda",
                true
            };

        case HardwareDeviceType::QSV:
            return HardwareBackendProfile{
                HardwareDeviceType::QSV,
                "qsv",
                AV_PIX_FMT_QSV,
                "scale_qsv",
                true
            };

        case HardwareDeviceType::VAAPI:
            return HardwareBackendProfile{
                HardwareDeviceType::VAAPI,
                "vaapi",
                AV_PIX_FMT_VAAPI,
                "scale_vaapi",
                true
            };

        case HardwareDeviceType::DRM:
            return HardwareBackendProfile{
                HardwareDeviceType::DRM,
                "drm",
                AV_PIX_FMT_DRM_PRIME,
                nullptr,
                false
            };

        case HardwareDeviceType::VideoToolbox:
            return HardwareBackendProfile{
                HardwareDeviceType::VideoToolbox,
                "videotoolbox",
                AV_PIX_FMT_VIDEOTOOLBOX,
                nullptr,
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

} // namespace media::ffmpeg
