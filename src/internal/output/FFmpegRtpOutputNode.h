#pragma once

#include "internal/FFmpegError.h"
#include "internal/output/FFmpegRtpMuxer.h"
#include "internal/output/PacketOutputNode.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegRtpOutputNode final : public PacketOutputNode {
public:
    FFmpegRtpOutputNode() = default;
    ~FFmpegRtpOutputNode() override = default;

    FFmpegRtpOutputNode(const FFmpegRtpOutputNode&) = delete;
    FFmpegRtpOutputNode& operator=(const FFmpegRtpOutputNode&) = delete;

    FFmpegRtpOutputNode(FFmpegRtpOutputNode&&) = delete;
    FFmpegRtpOutputNode& operator=(FFmpegRtpOutputNode&&) = delete;

    Status open(const FFmpegRtpOutputConfig& config)
    {
        return m_muxer.open(config);
    }

    Status writeHeader()
    {
        return m_muxer.writeHeader();
    }

    Status writeSdp()
    {
        return m_muxer.writeSdp();
    }

    Status writeTrailer()
    {
        return m_muxer.writeTrailer();
    }

    void reset()
    {
        m_muxer.reset();
    }

    AVFormatContext* context() const
    {
        return m_muxer.context();
    }

    const std::string& url() const
    {
        return m_muxer.url();
    }

    bool isOpen() const
    {
        return m_muxer.isOpen();
    }

    bool headerWritten() const
    {
        return m_muxer.headerWritten();
    }

    Status pushPacket(AVPacket* packet) override
    {
        AVFormatContext* fmtCtx = m_muxer.context();
        if (!fmtCtx) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegRtpOutputNode pushPacket failed: muxer is not open"));
        }

        if (!m_muxer.headerWritten()) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegRtpOutputNode pushPacket failed: muxer header is not written"));
        }

        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegRtpOutputNode pushPacket failed: packet is null"));
        }

        const int ret = av_interleaved_write_frame(fmtCtx, packet);
        if (ret < 0) {
            return Status::failure(makeFFmpegError("RTP packet write failed", ret));
        }

        return Status::success();
    }

private:
    FFmpegRtpMuxer m_muxer;
};

} // namespace media::ffmpeg
