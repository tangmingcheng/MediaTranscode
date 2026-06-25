#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/output/FFmpegRtpUrlBuilder.h"
#include "media_transcode/Result.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegRtpMuxer {
public:
    FFmpegRtpMuxer() = default;
    ~FFmpegRtpMuxer();

    FFmpegRtpMuxer(const FFmpegRtpMuxer&) = delete;
    FFmpegRtpMuxer& operator=(const FFmpegRtpMuxer&) = delete;
    FFmpegRtpMuxer(FFmpegRtpMuxer&&) = delete;
    FFmpegRtpMuxer& operator=(FFmpegRtpMuxer&&) = delete;

    Status open(const FFmpegRtpOutputConfig& config);
    Status writeHeader();
    Status writeSdp();
    Status writeTrailer();
    void reset();

    AVFormatContext* context() const;
    const std::string& url() const;
    bool isOpen() const;
    bool headerWritten() const;

private:
    FFmpegRtpOutputConfig m_config;
    std::string m_url;
    OutputFormatContextPtr m_context;
    bool m_headerWritten = false;
};

} // namespace media::ffmpeg
