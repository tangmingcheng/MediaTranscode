#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class FFmpegVideoHardwareTransferStage {
public:
    struct Config {
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
    bool m_zeroCopyPipeline = false;
    bool m_initialized = false;
    mutable int64_t m_transferLogCount = 0;
};

} // namespace media::ffmpeg
