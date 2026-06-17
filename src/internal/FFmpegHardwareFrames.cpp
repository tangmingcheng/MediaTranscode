#include "internal/FFmpegHardwareFrames.h"

#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg {

    HardwareFramesContext::~HardwareFramesContext()
    {
        reset();
    }

    HardwareFramesContext::HardwareFramesContext(HardwareFramesContext&& other) noexcept
    {
        *this = std::move(other);
    }

    HardwareFramesContext& HardwareFramesContext::operator=(HardwareFramesContext&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        reset();

        m_framesCtx = std::move(other.m_framesCtx);
        m_hardwareFormat = other.m_hardwareFormat;
        m_softwareFormat = other.m_softwareFormat;
        m_width = other.m_width;
        m_height = other.m_height;

        other.m_hardwareFormat = AV_PIX_FMT_NONE;
        other.m_softwareFormat = AV_PIX_FMT_NONE;
        other.m_width = 0;
        other.m_height = 0;

        return *this;
    }

    void HardwareFramesContext::reset()
    {
        m_framesCtx.reset();
        m_hardwareFormat = AV_PIX_FMT_NONE;
        m_softwareFormat = AV_PIX_FMT_NONE;
        m_width = 0;
        m_height = 0;
    }

    bool HardwareFramesContext::initialize(const HardwareDeviceContext& deviceContext,
                                           AVPixelFormat hardwareFormat,
                                           AVPixelFormat softwareFormat,
                                           int width,
                                           int height,
                                           int initialPoolSize,
                                           std::string* error)
    {
        reset();

        if (!deviceContext.isInitialized()) {
            if (error) {
                *error = "hardware frames initialization failed: device context is not initialized";
            }
            return false;
        }

        if (hardwareFormat == AV_PIX_FMT_NONE || softwareFormat == AV_PIX_FMT_NONE) {
            if (error) {
                *error = "hardware frames initialization failed: invalid pixel format";
            }
            return false;
        }

        if (width <= 0 || height <= 0) {
            if (error) {
                *error = "hardware frames initialization failed: invalid frame size";
            }
            return false;
        }

        m_framesCtx.reset(av_hwframe_ctx_alloc(deviceContext.raw()));
        if (!m_framesCtx) {
            if (error) {
                *error = "av_hwframe_ctx_alloc failed";
            }
            return false;
        }

        auto* frames = reinterpret_cast<AVHWFramesContext*>(m_framesCtx->data);
        frames->format = hardwareFormat;
        frames->sw_format = softwareFormat;
        frames->width = width;
        frames->height = height;
        frames->initial_pool_size = std::max(0, initialPoolSize);

        const int ret = av_hwframe_ctx_init(m_framesCtx.get());
        if (ret < 0) {
            if (error) {
                *error = "av_hwframe_ctx_init failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        m_hardwareFormat = hardwareFormat;
        m_softwareFormat = softwareFormat;
        m_width = width;
        m_height = height;

        return true;
    }

    bool HardwareFramesContext::isInitialized() const
    {
        return m_framesCtx != nullptr;
    }

    AVPixelFormat HardwareFramesContext::hardwareFormat() const
    {
        return m_hardwareFormat;
    }

    AVPixelFormat HardwareFramesContext::softwareFormat() const
    {
        return m_softwareFormat;
    }

    int HardwareFramesContext::width() const
    {
        return m_width;
    }

    int HardwareFramesContext::height() const
    {
        return m_height;
    }

    BufferRefPtr HardwareFramesContext::ref() const
    {
        return makeBufferRef(m_framesCtx.get());
    }

    AVBufferRef* HardwareFramesContext::raw() const
    {
        return m_framesCtx.get();
    }

} // namespace media::ffmpeg
