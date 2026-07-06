#pragma once

#include "internal/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <string>

namespace media::ffmpeg::graph {

struct FFmpegRealtimeInputSessionOptions {
    std::string url;
    std::string sdpText;
    std::string sdpPath;
    int readTimeoutMs = 5000;
};

class FFmpegRealtimeInputSession final {
public:
    FFmpegRealtimeInputSession() = default;
    ~FFmpegRealtimeInputSession();

    FFmpegRealtimeInputSession(const FFmpegRealtimeInputSession&) = delete;
    FFmpegRealtimeInputSession& operator=(const FFmpegRealtimeInputSession&) = delete;

    ::media::Status open(FFmpegRealtimeInputSessionOptions options);
    void close() noexcept;
    void interrupt() noexcept;

    AVFormatContext* context() noexcept;
    const AVFormatContext* context() const noexcept;
    ::media::ffmpeg::InputFormatContextPtr takeContext() noexcept;

private:
    static int interruptCallback(void* opaque) noexcept;

private:
    FFmpegRealtimeInputSessionOptions m_options;
    ::media::ffmpeg::InputFormatContextPtr m_context;
    std::string m_tempSdpPath;
    std::atomic_bool m_interrupted { false };
};

} // namespace media::ffmpeg::graph
