#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegHardwareEncoderSelector.h"
#include "internal/FFmpegPipelinePlanner.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

class FFmpegVideoEncoderStage {
public:
    struct Config {
        const TranscodeConfig* transcodeConfig = nullptr;
        const HardwarePipelinePlan* hardwarePlan = nullptr;

        AVStream* inputVideoStream = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        int inputWidth = 0;
        int inputHeight = 0;

        /*
         * FFmpegVideoEncoderStage does not own this hardware device context.
         * The decoder stage owns the device; the encoder only keeps an FFmpeg
         * reference copied from it when hardware encoding is selected.
         */
        const HardwareDeviceContext* hardwareDeviceContext = nullptr;
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

    bool hasHardwarePlan() const;
    bool hardwareDeviceAttached() const;
    bool zeroCopyPipeline() const;

    const HardwareBackendProfile& hardwareBackend() const;
    const HardwareEncoderSelection& hardwareEncoderSelection() const;

private:
    bool openEncoderAndCreateOutputStream(std::string* error);
    bool initializeHardwareDeviceForEncoder(const AVCodec* encoder, std::string* error);

private:
    TranscodeConfig m_config;

    AVStream* m_inputVideoStream = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    int m_inputWidth = 0;
    int m_inputHeight = 0;

    AVCodecContext* m_encoderCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;

    const HardwareDeviceContext* m_hardwareDeviceContext = nullptr;
    HardwarePipelinePlan m_hardwarePlan;
    HardwareBackendProfile m_hardwareBackend;
    HardwareEncoderSelection m_hardwareEncoderSelection;

    bool m_hasHardwarePlan = false;
    bool m_decoderUsesHardwareFrames = false;
    bool m_decoderHardwareDeviceAttached = false;
    bool m_hardwareDeviceAttachedToEncoder = false;
    bool m_zeroCopyPipeline = false;

    int m_outputFps = 0;
    bool m_enableConstantFps = false;
};

} // namespace media::ffmpeg
