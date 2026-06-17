#pragma once

#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegHardwareDecoder.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

    class FFmpegVideoDecoderStage {
    public:
        enum class ReceiveFrameState {
            Frame,
            NeedMoreInput
        };

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

        Status initialize(const Config& config);
        Status sendPacket(AVPacket* packet);
        Status sendFlush();

        Result<ReceiveFrameState> receiveFrame(AVFrame* frame);

        bool isInitialized() const;
        AVCodecContext* context() const;

        bool hasHardwarePlan() const;
        bool usesHardwareFrames() const;
        bool hardwareDeviceAttached() const;
        const HardwareDecoderSupport::Config& hardwareDecoderConfig() const;
        const HardwareDeviceContext& hardwareDeviceContext() const;

    private:
        Status initializeHardwareDevice(const AVCodec* decoder);

        static AVPixelFormat selectDecoderPixelFormat(AVCodecContext* ctx,
                                                      const AVPixelFormat* formats);

    private:
        AVStream* m_inputStream = nullptr;
        CodecContextPtr m_decoderCtx;

        HardwareDeviceContext m_hardwareDeviceContext;
        HardwareDecoderSupport::Config m_hardwareDecoderConfig;
        bool m_hasHardwarePlan = false;
        bool m_hardwareDeviceAttached = false;
        bool m_usesHardwareFrames = false;
    };

} // namespace media::ffmpeg
