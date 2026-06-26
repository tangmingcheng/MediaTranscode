#pragma once

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

    void reset();
    Status initialize(const Config& config);
    Status writePacket(AVPacket* packet) override;

    bool isInitialized() const;

private:
    AVFormatContext* m_outputFmtCtx = nullptr;
};

} // namespace media::ffmpeg
