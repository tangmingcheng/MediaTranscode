#pragma once

#include <string>
#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

class FFmpegVideoPipeline {
public:
    struct Config {
        AVCodecContext* encoderCtx = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        AVStream* outputVideoStream = nullptr;
    };

    FFmpegVideoPipeline() = default;
    ~FFmpegVideoPipeline();

    FFmpegVideoPipeline(const FFmpegVideoPipeline&) = delete;
    FFmpegVideoPipeline& operator=(const FFmpegVideoPipeline&) = delete;

    FFmpegVideoPipeline(FFmpegVideoPipeline&&) noexcept;
    FFmpegVideoPipeline& operator=(FFmpegVideoPipeline&&) noexcept;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool sendFrame(AVFrame* frame, std::string* error);
    int receiveAndWritePackets(std::string* error);

    bool isInitialized() const;

private:
    AVCodecContext* m_encoderCtx = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;
};

} // namespace media::ffmpeg