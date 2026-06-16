#pragma once

#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegHardwareEncoderSelector.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

    class FFmpegVideoEncoderStage {
    public:
        struct Config {
            const TranscodeConfig* transcodeConfig = nullptr;
            const HardwarePipelinePlan* hardwarePlan = nullptr;
            AVCodecContext* decoderCtx = nullptr;
            AVStream* inputVideoStream = nullptr;
            AVFormatContext* outputFmtCtx = nullptr;
            const HardwareDeviceContext* sharedHardwareDevice = nullptr;
            bool decoderUsesHardwareFrames = false;
            bool decoderHardwareDeviceAttached = false;
        };

        FFmpegVideoEncoderStage() = default;
        ~FFmpegVideoEncoderStage();

        FFmpegVideoEncoderStage(const FFmpegVideoEncoderStage&) = delete;
        FFmpegVideoEncoderStage& operator=(const FFmpegVideoEncoderStage&) = delete;
        FFmpegVideoEncoderStage(FFmpegVideoEncoderStage&&) = delete;
        FFmpegVideoEncoderStage& operator=(FFmpegVideoEncoderStage&&) = delete;

        void reset();

        bool initialize(const Config& config, std::string* error);

        bool isInitialized() const;
        AVCodecContext* context() const;
        AVStream* outputStream() const;

        int outputFps() const;
        bool enableConstantFps() const;
        bool hardwareDeviceAttached() const;
        bool zeroCopyPipeline() const;

    private:
        bool initializeHardwareDeviceForEncoder(const AVCodec* encoder,
                                                const HardwareDeviceContext* sharedHardwareDevice,
                                                std::string* error);

    private:
        TranscodeConfig m_config;
        HardwarePipelinePlan m_hardwarePlan;
        HardwareEncoderSelection m_hardwareEncoderSelection;
        bool m_hasHardwarePlan = false;

        AVCodecContext* m_encoderCtx = nullptr;
        AVStream* m_inputVideoStream = nullptr;
        AVFormatContext* m_outputFmtCtx = nullptr;
        AVStream* m_outputVideoStream = nullptr;

        int m_outputFps = 0;
        bool m_enableConstantFps = false;
        bool m_hardwareDeviceAttached = false;
        bool m_zeroCopyPipeline = false;
    };

} // namespace media::ffmpeg
