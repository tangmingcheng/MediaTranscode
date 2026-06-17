#pragma once

#include "internal/FFmpegRAII.h"

#include <cstdint>
#include <deque>
#include <string>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

class FFmpegVideoFrameRateStage {
public:
    struct Config {
        AVRational inputTimeBase{ 0, 1 };
        int targetFps = 0;
    };

    FFmpegVideoFrameRateStage() = default;
    ~FFmpegVideoFrameRateStage();

    FFmpegVideoFrameRateStage(const FFmpegVideoFrameRateStage&) = delete;
    FFmpegVideoFrameRateStage& operator=(const FFmpegVideoFrameRateStage&) = delete;
    FFmpegVideoFrameRateStage(FFmpegVideoFrameRateStage&&) = delete;
    FFmpegVideoFrameRateStage& operator=(FFmpegVideoFrameRateStage&&) = delete;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool sendFrame(AVFrame* frame, std::string* error);
    bool flush(std::string* error);

    int receiveFrame(AVFrame* frame, std::string* error);

    bool isInitialized() const;
    bool enabled() const;
    AVRational inputTimeBase() const;
    int targetFps() const;

private:
    int64_t targetPtsForIndex(int64_t outputIndex) const;
    int64_t targetFrameDuration() const;
    bool queueFrameReference(AVFrame* sourceFrame,
                             int64_t outputPts,
                             std::string* error);
    bool queuePassthroughFrame(AVFrame* frame, std::string* error);
    bool rememberLastInputFrame(AVFrame* frame, std::string* error);
    AVFrame* chooseSourceFrameForTarget(AVFrame* currentFrame,
                                        int64_t currentPts,
                                        int64_t targetPts) const;

private:
    AVRational m_inputTimeBase{ 0, 1 };
    int m_targetFps = 0;
    bool m_initialized = false;
    bool m_started = false;
    bool m_flushed = false;

    int64_t m_startPts = 0;
    int64_t m_nextOutputIndex = 0;
    int64_t m_lastInputPts = 0;
    int64_t m_lastOutputPts = AV_NOPTS_VALUE;

    FramePtr m_lastInputFrame;
    std::deque<FramePtr> m_pendingFrames;
};

} // namespace media::ffmpeg
