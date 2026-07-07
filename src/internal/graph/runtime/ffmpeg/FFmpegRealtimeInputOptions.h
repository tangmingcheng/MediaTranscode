#pragma once

#include <string>

struct AVDictionary;

namespace media::ffmpeg::graph {

struct FFmpegRealtimeInputOptions {
    std::string rtspTransport;
    int openTimeoutMs = 0;
    int readTimeoutMs = 0;
    int analyzeDurationUs = 0;
    int probeSizeBytes = 0;
    bool lowLatency = false;
    bool allowFileUdpRtpProtocols = false;
};

void applyFFmpegRealtimeInputOptions(AVDictionary** dictionary,
                                     const FFmpegRealtimeInputOptions& options);

} // namespace media::ffmpeg::graph
