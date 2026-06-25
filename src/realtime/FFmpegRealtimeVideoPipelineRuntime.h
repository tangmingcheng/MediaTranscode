#pragma once

#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"
#include "media_transcode/Result.h"

#include <memory>

struct AVFormatContext;
struct AVPacket;
struct AVStream;

namespace media {

class FFmpegRealtimeVideoPipelineRuntime {
public:
    struct Config {
        const RealtimeCoreConfig* realtimeConfig = nullptr;
        AVFormatContext* inputFormatContext = nullptr;
        AVStream* inputVideoStream = nullptr;
    };

    FFmpegRealtimeVideoPipelineRuntime();
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
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace media
