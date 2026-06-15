#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
}

namespace media::ffmpeg {

    /*
     * HardwareDeviceContext only owns the FFmpeg hardware device context.
     *
     * It intentionally does not decide encoder order, filter description, or
     * software fallback policy. Those decisions belong to higher-level pipeline
     * code. Keeping this class focused prevents hardware policies from being
     * scattered into FFmpegUtils.cpp.
     */
    class HardwareDeviceContext {
    public:
        HardwareDeviceContext() = default;
        ~HardwareDeviceContext();

        HardwareDeviceContext(const HardwareDeviceContext&) = delete;
        HardwareDeviceContext& operator=(const HardwareDeviceContext&) = delete;

        HardwareDeviceContext(HardwareDeviceContext&& other) noexcept;
        HardwareDeviceContext& operator=(HardwareDeviceContext&& other) noexcept;

        void reset();

        bool initialize(HardwareDeviceType requestedDevice,
                        const AVCodec* decoder,
                        const AVCodec* encoder,
                        std::string* error);

        bool isInitialized() const;
        AVHWDeviceType avDeviceType() const;
        HardwareDeviceType requestedDeviceType() const;
        HardwareDeviceType resolvedDeviceType() const;
        const std::string& resolvedDeviceName() const;

        /*
         * Caller owns the returned reference and must av_buffer_unref() it.
         */
        AVBufferRef* ref() const;
        AVBufferRef* raw() const;

        static AVHWDeviceType toAVDeviceType(HardwareDeviceType type);
        static HardwareDeviceType fromAVDeviceType(AVHWDeviceType type);
        static const char* toAVDeviceName(HardwareDeviceType type);
        static HardwareDeviceType inferDeviceType(const AVCodec* decoder,
                                                  const AVCodec* encoder);
        static bool isExplicitHardwareDevice(HardwareDeviceType type);

    private:
        AVBufferRef* m_deviceCtx = nullptr;
        AVHWDeviceType m_avDeviceType = AV_HWDEVICE_TYPE_NONE;
        HardwareDeviceType m_requestedDeviceType = HardwareDeviceType::None;
        HardwareDeviceType m_resolvedDeviceType = HardwareDeviceType::None;
        std::string m_resolvedDeviceName;
    };

} // namespace media::ffmpeg
