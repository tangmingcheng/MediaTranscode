#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class FFmpegVideoHardwareTransferStage {
public:
    struct Config {
        AVCodecContext* decoderCtx = nullptr;
        bool zeroCopyPipeline = false;
    };

    FFmpegVideoHardwareTransferStage() = default;
    ~FFmpegVideoHardwareTransferStage();

    FFmpegVideoHardwareTransferStage(const FFmpegVideoHardwareTransferStage&) = delete;
    FFmpegVideoHardwareTransferStage& operator=(const FFmpegVideoHardwareTransferStage&) = delete;
    FFmpegVideoHardwareTransferStage(FFmpegVideoHardwareTransferStage&&) = delete;
    FFmpegVideoHardwareTransferStage& operator=(FFmpegVideoHardwareTransferStage&&) = delete;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool transferToSoftware(AVFrame* hardwareFrame,
                            AVFrame* softwareFrame,
                            std::string* error) const;

    bool isInitialized() const;

private:
    AVCodecContext* m_decoderCtx = nullptr;
    bool m_zeroCopyPipeline = false;
    bool m_initialized = false;
};

} // namespace media::ffmpeg
