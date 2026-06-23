#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "media_transcode/Result.h"
#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegHardwareContext.h"
#include "internal/FFmpegHardwareEncoderSelector.h"
#include "internal/FFmpegHardwareFrames.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegVideoAdapter.h"
#include "internal/FFmpegVideoInputMetadata.h"

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

        FFmpegVideoInputMetadata inputMetadata;
        AVFormatContext* outputFmtCtx = nullptr;

        /*
         * FFmpegVideoEncoderStage does not own this hardware device context.
         * The decoder stage owns the device; the encoder only keeps FFmpeg
         * references copied from it when hardware encoding is selected.
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

    Status initialize(const Config& config);

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
    Status openEncoderAndCreateOutputStream();
    Status initializeHardwareDeviceForEncoder(const AVCodec* encoder);
    Status initializeHardwareFramesContextForEncoder();

private:
    TranscodeConfig m_config;

    FFmpegVideoInputMetadata m_inputMetadata;
    AVFormatContext* m_outputFmtCtx = nullptr;

    CodecContextPtr m_encoderCtx;
    AVStream* m_outputVideoStream = nullptr;

    const HardwareDeviceContext* m_hardwareDeviceContext = nullptr;
    bool m_decoderUsesHardwareFrames = false;
    bool m_decoderHardwareDeviceAttached = false;

    AVBufferRefPtr m_encoderHardwareFramesContext;

    int m_outputFps = 25;
    bool m_enableConstantFps = false;
};

} // namespace media::ffmpeg
