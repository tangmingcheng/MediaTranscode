#pragma once

#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegVideoTranscodePipeline.h"
#include "internal/output/FFmpegRtpMuxer.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media {

class FFmpegRealtimeVideoPipelineRuntime {
public:
    struct Config {
        const RealtimeCoreConfig* realtimeConfig = nullptr;
        AVFormatContext* inputFormatContext = nullptr;
        AVStream* inputVideoStream = nullptr;
    };

    FFmpegRealtimeVideoPipelineRuntime() = default;
    ~FFmpegRealtimeVideoPipelineRuntime();

    FFmpegRealtimeVideoPipelineRuntime(const FFmpegRealtimeVideoPipelineRuntime&) = delete;
    FFmpegRealtimeVideoPipelineRuntime& operator=(const FFmpegRealtimeVideoPipelineRuntime&) = delete;
    FFmpegRealtimeVideoPipelineRuntime(FFmpegRealtimeVideoPipelineRuntime&&) = delete;
    FFmpegRealtimeVideoPipelineRuntime& operator=(FFmpegRealtimeVideoPipelineRuntime&&) = delete;

    Status initialize(const Config& config);
    Status processPacket(AVPacket* packet);
    Status finish(bool flush);
    void reset();

    RealtimeCoreStats stats() const;

private:
    Status planPipeline();
    Status initializeMuxerAndPipeline();

private:
    RealtimeCoreConfig m_realtimeConfig;
    TranscodeConfig m_pipelineConfig;

    AVFormatContext* m_inputFormatContext = nullptr;
    AVStream* m_inputVideoStream = nullptr;

    ffmpeg::TimelineNormalizer m_timeline;
    ffmpeg::HardwarePipelinePlan m_hardwarePlan;
    const ffmpeg::HardwarePipelinePlan* m_executionPlan = nullptr;
    ffmpeg::FFmpegRtpMuxer m_rtpMuxer;
    ffmpeg::FFmpegVideoTranscodePipeline m_videoPipeline;

    RealtimeCoreStats m_stats;
    bool m_initialized = false;
    bool m_finished = false;
};

} // namespace media
