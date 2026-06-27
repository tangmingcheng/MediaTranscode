#pragma once

#include "internal/FFmpegError.h"
#include "internal/graph/nodes/output/packet/PacketOutputNode.h"
#include "internal/output/capabilities/video/VideoOutputStreamProvider.h"
#include "internal/output/muxers/rtp/FFmpegRtpMuxer.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegRtpOutputNode final : public PacketOutputNode,
                                  public VideoOutputStreamProvider {
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

    bool requiresGlobalHeader() const override
    {
        AVFormatContext* fmtCtx = m_muxer.context();
        return fmtCtx &&
            fmtCtx->oformat &&
            (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER);
    }

    Result<AVStream*> createVideoStream(AVCodecContext* encoderCtx) override
    {
        AVFormatContext* fmtCtx = m_muxer.context();
        if (!fmtCtx) {
            return Result<AVStream*>::failure(ErrorInfo::notInitialized(
                "FFmpegRtpOutputNode createVideoStream failed: muxer is not open"));
        }

        if (!encoderCtx) {
            return Result<AVStream*>::failure(ErrorInfo::invalidArgument(
                "FFmpegRtpOutputNode createVideoStream failed: encoderCtx is null"));
        }

        AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
        if (!stream) {
            return Result<AVStream*>::failure(makeAllocationError(
                "avformat_new_stream RTP video failed"));
        }

        stream->time_base = encoderCtx->time_base;

        const int ret = avcodec_parameters_from_context(stream->codecpar, encoderCtx);
        if (ret < 0) {
            return Result<AVStream*>::failure(makeFFmpegError(
                "avcodec_parameters_from_context RTP video failed", ret));
        }

        stream->codecpar->codec_tag = 0;
        return Result<AVStream*>::success(stream);
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
