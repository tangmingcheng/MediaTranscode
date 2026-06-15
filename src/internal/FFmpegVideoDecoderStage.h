#pragma once

#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegHardwareDecoder.h"
#include "internal/FFmpegPipelinePlanner.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    class FFmpegVideoDecoderStage {
    public:
        struct Config {
            AVStream* inputStream = nullptr;
            const HardwarePipelinePlan* hardwarePlan = nullptr;
        };

        FFmpegVideoDecoderStage() = default;
        ~FFmpegVideoDecoderStage();

        FFmpegVideoDecoderStage(const FFmpegVideoDecoderStage&) = delete;
        FFmpegVideoDecoderStage& operator=(const FFmpegVideoDecoderStage&) = delete;
        FFmpegVideoDecoderStage(FFmpegVideoDecoderStage&&) = delete;
        FFmpegVideoDecoderStage& operator=(FFmpegVideoDecoderStage&&) = delete;

        void reset();

        bool initialize(const Config& config, std::string* error);
        bool sendPacket(AVPacket* packet, std::string* error);
        bool sendFlush(std::string* error);

        // Returns 1 when a frame is received, 0 when the decoder needs more input or reached EOF,
        // and -1 on error.
        int receiveFrame(AVFrame* frame, std::string* error);

        bool isInitialized() const;
        AVCodecContext* context() const;

        bool hasHardwarePlan() const;
        bool usesHardwareFrames() const;
        bool hardwareDeviceAttached() const;
        const HardwareDecoderSupport::Config& hardwareDecoderConfig() const;
        const HardwareDeviceContext& hardwareDeviceContext() const;

    private:
        bool initializeHardwareDevice(const AVCodec* decoder, std::string* error);

        static AVPixelFormat selectDecoderPixelFormat(AVCodecContext* ctx,
                                                      const AVPixelFormat* formats);

    private:
        AVStream* m_inputStream = nullptr;
        AVCodecContext* m_decoderCtx = nullptr;

        HardwareDeviceContext m_hardwareDeviceContext;
        HardwareDecoderSupport::Config m_hardwareDecoderConfig;
        bool m_hasHardwarePlan = false;
        bool m_hardwareDeviceAttached = false;
        bool m_usesHardwareFrames = false;
    };

} // namespace media::ffmpeg
