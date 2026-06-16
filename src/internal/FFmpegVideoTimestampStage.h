#pragma once

#include "internal/FFmpegVideoInputMetadata.h"

#include <cstdint>
#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

class TimelineNormalizer;

class FFmpegVideoTimestampStage {
public:
    struct Config {
        FFmpegVideoInputMetadata inputMetadata;
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
    AVRational m_inputTimeBase{ 0, 1 };
    TimelineNormalizer* m_timeline = nullptr;
};

} // namespace media::ffmpeg
