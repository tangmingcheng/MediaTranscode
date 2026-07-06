#pragma once

#include "internal/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct FFmpegRtpOutputSessionOptions {
    std::string url;
    int writeTimeoutMs = 5000;
};

class FFmpegRtpOutputSession final {
public:
    FFmpegRtpOutputSession() = default;
    ~FFmpegRtpOutputSession();

    FFmpegRtpOutputSession(const FFmpegRtpOutputSession&) = delete;
    FFmpegRtpOutputSession& operator=(const FFmpegRtpOutputSession&) = delete;

    ::media::Status open(FFmpegRtpOutputSessionOptions options);
    void close() noexcept;
    void interrupt() noexcept;

    AVFormatContext* context() noexcept;
    ::media::ffmpeg::OutputFormatContextPtr takeContext() noexcept;

private:
    static int interruptCallback(void* opaque) noexcept;

private:
    FFmpegRtpOutputSessionOptions m_options;
    ::media::ffmpeg::OutputFormatContextPtr m_context;
    bool m_interrupted = false;
};

} // namespace media::ffmpeg::graph
