#pragma once

#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegHardwareVideoFilterGraph.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegVideoFilterGraph.h"
#include "internal/FFmpegVideoInputMetadata.h"

#include <cstdint>
#include <deque>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg {

class FFmpegVideoFilterStage {
public:
    struct Config {
        AVCodecContext* encoderCtx = nullptr;
        FFmpegVideoInputMetadata inputMetadata;

        int outputFps = 0;
        bool enableConstantFps = false;

        bool zeroCopyPipeline = false;
        HardwareBackendProfile hardwareBackend;
    };

    FFmpegVideoFilterStage() = default;
    ~FFmpegVideoFilterStage();

    FFmpegVideoFilterStage(const FFmpegVideoFilterStage&) = delete;
    FFmpegVideoFilterStage& operator=(const FFmpegVideoFilterStage&) = delete;
    FFmpegVideoFilterStage(FFmpegVideoFilterStage&&) = delete;
    FFmpegVideoFilterStage& operator=(FFmpegVideoFilterStage&&) = delete;

    void reset();

    bool initialize(const Config& config, std::string* error);

    bool sendSoftwareFrame(AVFrame* frame, std::string* error);
    bool sendHardwareFrame(AVFrame* frame, std::string* error);
    bool flush(std::string* error);

    // Returns 1 when a filtered frame is received, 0 when the filter graph needs
    // more input or reached EOF, and -1 on error.
    int receiveFrame(AVFrame* frame, std::string* error);

    bool isInitialized() const;
    bool zeroCopyPipeline() const;
    bool hardwareFilterGraphInitialized() const;

private:
    bool initializeSoftwareFilterGraph(const AVFrame* firstFrame,
                                       std::string* error);
    bool initializeHardwareFilterGraphFromFrame(const AVFrame* frame, std::string* error);

    bool queueBypassedHardwareFrame(AVFrame* frame, std::string* error);
    int receiveBypassedHardwareFrame(AVFrame* frame, std::string* error);
    void clearBypassedHardwareFrames();

    int receiveSoftwareFrame(AVFrame* frame, std::string* error);
    int receiveHardwareFrame(AVFrame* frame, std::string* error);
    bool rescaleAndValidateFramePts(AVFrame* frame,
                                    AVRational filterTimeBase,
                                    bool dropNonIncreasingFrame,
                                    std::string* error);

private:
    AVCodecContext* m_encoderCtx = nullptr;
    FFmpegVideoInputMetadata m_inputMetadata;

    int m_outputFps = 0;
    bool m_enableConstantFps = false;

    bool m_initialized = false;
    bool m_zeroCopyPipeline = false;
    bool m_bypassHardwareFilterGraph = false;
    bool m_softwareFilterGraphInitialized = false;
    bool m_hardwareFilterGraphInitialized = false;

    HardwareBackendProfile m_hardwareBackend;
    VideoFilterGraph m_filterGraph;
    HardwareVideoFilterGraph m_hardwareFilterGraph;
    std::deque<FramePtr> m_bypassedHardwareFrames;

    int64_t m_lastSubmittedPts = AV_NOPTS_VALUE;
    int64_t m_bypassedHardwareFrameLogCount = 0;
    int64_t m_filteredHardwareFrameLogCount = 0;
};

} // namespace media::ffmpeg
