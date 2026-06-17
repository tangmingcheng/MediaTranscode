#pragma once

#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegRAII.h"

#include <string>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    /*
     * Owns an AVHWFramesContext.
     *
     * This module is deliberately separate from HardwareDeviceContext because
     * one device can back multiple frame pools with different sizes/formats.
     */
    class HardwareFramesContext {
    public:
        HardwareFramesContext() = default;
        ~HardwareFramesContext();

        HardwareFramesContext(const HardwareFramesContext&) = delete;
        HardwareFramesContext& operator=(const HardwareFramesContext&) = delete;

        HardwareFramesContext(HardwareFramesContext&& other) noexcept;
        HardwareFramesContext& operator=(HardwareFramesContext&& other) noexcept;

        void reset();

        bool initialize(const HardwareDeviceContext& deviceContext,
                        AVPixelFormat hardwareFormat,
                        AVPixelFormat softwareFormat,
                        int width,
                        int height,
                        int initialPoolSize,
                        std::string* error);

        bool isInitialized() const;
        AVPixelFormat hardwareFormat() const;
        AVPixelFormat softwareFormat() const;
        int width() const;
        int height() const;

        /*
         * Returns an owning reference to the underlying AVBufferRef.
         */
        BufferRefPtr ref() const;
        AVBufferRef* raw() const;

    private:
        BufferRefPtr m_framesCtx;
        AVPixelFormat m_hardwareFormat = AV_PIX_FMT_NONE;
        AVPixelFormat m_softwareFormat = AV_PIX_FMT_NONE;
        int m_width = 0;
        int m_height = 0;
    };

} // namespace media::ffmpeg
