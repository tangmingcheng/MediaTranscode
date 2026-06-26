#pragma once

#include "internal/FFmpegError.h"
#include "internal/output/PacketOutputNode.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegMuxerOutputNode final : public PacketOutputNode {
public:
    struct Config {
        AVFormatContext* outputFmtCtx = nullptr;
    };

    FFmpegMuxerOutputNode() = default;
    ~FFmpegMuxerOutputNode() override = default;

    void reset()
    {
        m_outputFmtCtx = nullptr;
    }

    Status initialize(const Config& config)
    {
        reset();
        if (!config.outputFmtCtx) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode initialize failed: outputFmtCtx is null"));
        }
        m_outputFmtCtx = config.outputFmtCtx;
        return Status::success();
    }

    Status pushPacket(AVPacket* packet) override
    {
        if (!m_outputFmtCtx) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerOutputNode pushPacket failed: not initialized"));
        }

        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode pushPacket failed: packet is null"));
        }

        const int ret = av_interleaved_write_frame(m_outputFmtCtx, packet);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "video packet write failed", ret));
        }

        return Status::success();
    }

private:
    AVFormatContext* m_outputFmtCtx = nullptr;
};

} // namespace media::ffmpeg
