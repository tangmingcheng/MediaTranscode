#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class TimelineNormalizer;

class FFmpegVideoTimestampStage {
public:
    struct Config {
        AVStream* inputVideoStream = nullptr;
        TimelineNormalizer* timeline = nullptr;
    };

    FFmpegVideoTimestampStage() = default;
    ~FFmpegVideoTimestampStage();

    FFmpegVideoTimestampStage(const FFmpegVideoTimestampStage&) = delete;
    FFmpegVideoTimestampStage& operator=(const FFmpegVideoTimestampStage&) = delete;
    FFmpegVideoTimestampStage(FFmpegVideoTimestampStage&&) = delete;
    FFmpegVideoTimestampStage& operator=(FFmpegVideoTimestampStage&&) = delete;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool normalizeFramePts(AVFrame* frame, std::string* error) const;

    bool isInitialized() const;

    static int64_t decodedFrameTimestamp(const AVFrame* frame);

private:
    AVStream* m_inputVideoStream = nullptr;
    TimelineNormalizer* m_timeline = nullptr;
};

} // namespace media::ffmpeg
