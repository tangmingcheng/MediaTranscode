#include "internal/FFmpegHardwareContext.h"

#include "internal/FFmpegUtils.h"

#include <utility>

namespace media::ffmpeg {
namespace {

    bool codecNameContains(const AVCodec* codec, const char* token)
    {
        if (!codec || !codec->name || !token) {
            return false;
        }

        return std::string(codec->name).find(token) != std::string::npos;
    }

} // namespace

    HardwareDeviceContext::~HardwareDeviceContext()
    {
        reset();
    }

    HardwareDeviceContext::HardwareDeviceContext(HardwareDeviceContext&& other) noexcept
    {
        *this = std::move(other);
    }

    HardwareDeviceContext& HardwareDeviceContext::operator=(HardwareDeviceContext&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        reset();

        m_deviceCtx = other.m_deviceCtx;
        m_avDeviceType = other.m_avDeviceType;
        m_requestedDeviceType = other.m_requestedDeviceType;
        m_resolvedDeviceType = other.m_resolvedDeviceType;
        m_resolvedDeviceName = std::move(other.m_resolvedDeviceName);

        other.m_deviceCtx = nullptr;
        other.m_avDeviceType = AV_HWDEVICE_TYPE_NONE;
        other.m_requestedDeviceType = HardwareDeviceType::None;
        other.m_resolvedDeviceType = HardwareDeviceType::None;
        other.m_resolvedDeviceName.clear();

        return *this;
    }

    void HardwareDeviceContext::reset()
    {
        if (m_deviceCtx) {
            av_buffer_unref(&m_deviceCtx);
        }

        m_avDeviceType = AV_HWDEVICE_TYPE_NONE;
        m_requestedDeviceType = HardwareDeviceType::None;
        m_resolvedDeviceType = HardwareDeviceType::None;
        m_resolvedDeviceName.clear();
    }

    bool HardwareDeviceContext::initialize(HardwareDeviceType requestedDevice,
                                           const AVCodec* decoder,
                                           const AVCodec* encoder,
                                           std::string* error)
    {
        reset();

        m_requestedDeviceType = requestedDevice;
        m_resolvedDeviceType = requestedDevice == HardwareDeviceType::Auto
            ? inferDeviceType(decoder, encoder)
            : requestedDevice;

        if (m_resolvedDeviceType == HardwareDeviceType::None) {
            if (error) {
                *error = "hardware device initialization skipped: no hardware device type selected";
            }
            return false;
        }

        m_avDeviceType = toAVDeviceType(m_resolvedDeviceType);
        if (m_avDeviceType == AV_HWDEVICE_TYPE_NONE) {
            if (error) {
                *error = "hardware device initialization failed: unsupported device type";
            }
            return false;
        }

        const int ret = av_hwdevice_ctx_create(
            &m_deviceCtx,
            m_avDeviceType,
            nullptr,
            nullptr,
            0
        );

        if (ret < 0) {
            if (error) {
                *error = "av_hwdevice_ctx_create failed [" +
                    std::string(toAVDeviceName(m_resolvedDeviceType)) +
                    "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        const char* deviceName = av_hwdevice_get_type_name(m_avDeviceType);
        m_resolvedDeviceName = deviceName ? deviceName : toAVDeviceName(m_resolvedDeviceType);

        return true;
    }

    bool HardwareDeviceContext::isInitialized() const
    {
        return m_deviceCtx != nullptr && m_avDeviceType != AV_HWDEVICE_TYPE_NONE;
    }

    AVHWDeviceType HardwareDeviceContext::avDeviceType() const
    {
        return m_avDeviceType;
    }

    HardwareDeviceType HardwareDeviceContext::requestedDeviceType() const
    {
        return m_requestedDeviceType;
    }

    HardwareDeviceType HardwareDeviceContext::resolvedDeviceType() const
    {
        return m_resolvedDeviceType;
    }

    const std::string& HardwareDeviceContext::resolvedDeviceName() const
    {
        return m_resolvedDeviceName;
    }

    AVBufferRef* HardwareDeviceContext::ref() const
    {
        return m_deviceCtx ? av_buffer_ref(m_deviceCtx) : nullptr;
    }

    AVBufferRef* HardwareDeviceContext::raw() const
    {
        return m_deviceCtx;
    }

    AVHWDeviceType HardwareDeviceContext::toAVDeviceType(HardwareDeviceType type)
    {
        const char* name = toAVDeviceName(type);
        if (!name || !*name) {
            return AV_HWDEVICE_TYPE_NONE;
        }

        return av_hwdevice_find_type_by_name(name);
    }

    const char* HardwareDeviceContext::toAVDeviceName(HardwareDeviceType type)
    {
        switch (type) {
        case HardwareDeviceType::D3D11VA:
            return "d3d11va";
        case HardwareDeviceType::CUDA:
            return "cuda";
        case HardwareDeviceType::QSV:
            return "qsv";
        case HardwareDeviceType::VAAPI:
            return "vaapi";
        case HardwareDeviceType::DRM:
            return "drm";
        case HardwareDeviceType::VideoToolbox:
            return "videotoolbox";
        case HardwareDeviceType::Auto:
        case HardwareDeviceType::None:
        default:
            return "";
        }
    }

    HardwareDeviceType HardwareDeviceContext::inferDeviceType(const AVCodec* decoder,
                                                              const AVCodec* encoder)
    {
        const AVCodec* preferredCodec = encoder ? encoder : decoder;

        if (codecNameContains(preferredCodec, "nvenc") ||
            codecNameContains(preferredCodec, "cuvid") ||
            codecNameContains(preferredCodec, "cuda")) {
            return HardwareDeviceType::CUDA;
        }

        if (codecNameContains(preferredCodec, "qsv")) {
            return HardwareDeviceType::QSV;
        }

        if (codecNameContains(preferredCodec, "vaapi")) {
            return HardwareDeviceType::VAAPI;
        }

        if (codecNameContains(preferredCodec, "videotoolbox")) {
            return HardwareDeviceType::VideoToolbox;
        }

        if (codecNameContains(preferredCodec, "d3d11va") ||
            codecNameContains(preferredCodec, "_mf")) {
            return HardwareDeviceType::D3D11VA;
        }

        if (codecNameContains(preferredCodec, "rkmpp")) {
            return HardwareDeviceType::DRM;
        }

        return HardwareDeviceType::None;
    }

    bool HardwareDeviceContext::isExplicitHardwareDevice(HardwareDeviceType type)
    {
        return type != HardwareDeviceType::None && type != HardwareDeviceType::Auto;
    }

} // namespace media::ffmpeg
