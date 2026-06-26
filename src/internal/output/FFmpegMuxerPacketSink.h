#pragma once

#include "internal/FFmpegError.h"
#include "internal/output/IEncodedPacketSink.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegMuxerPacketSink final : public IEncodedPacketSink {
public:
    struct Config {
        AVFormatContext* outputFmtCtx = nullptr;
    };

    FFmpegMuxerPacketSink() = default;
    ~FFmpegMuxerPacketSink() override = default;

    FFmpegMuxerPacketSink(const FFmpegMuxerPacketSink&) = delete;
    FFmpegMuxerPacketSink& operator=(const FFmpegMuxerPacketSink&) = delete;

    void reset()
    {
        m_outputFmtCtx = nullptr;
    }

    Status initialize(const Config& config)
    {
        reset();
        if (!config.outputFmtCtx) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerPacketSink initialize failed: outputFmtCtx is null"));
        }
        m_outputFmtCtx = config.outputFmtCtx;
        return Status::success();
    }

    Status writePacket(AVPacket* packet) override
    {
        if (!m_outputFmtCtx) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerPacketSink writePacket failed: not initialized"));
        }
        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerPacketSink writePacket failed: packet is null"));
        }

        const auto writeFrame = &av_interleaved_write_frame;
        const int ret = writeFrame(m_outputFmtCtx, packet);
        if (ret < 0) {
            return Status::failure(makeFFmpegError("video packet write failed", ret));
        }
        return Status::success();
    }

    bool isInitialized() const
    {
        return m_outputFmtCtx != nullptr;
    }

private:
    AVFormatContext* m_outputFmtCtx = nullptr;
};

} // namespace media::ffmpeg
