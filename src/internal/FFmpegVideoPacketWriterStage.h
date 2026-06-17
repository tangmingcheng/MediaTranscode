#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegVideoPacketWriterStage {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        /*
         * FFmpegVideoPacketWriterStage does not own these contexts.
         * EncoderStage owns encoderCtx and muxer setup owns outputFmtCtx/output stream.
         */
        AVCodecContext* encoderCtx = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        AVStream* outputVideoStream = nullptr;
    };

    FFmpegVideoPacketWriterStage() = default;
    ~FFmpegVideoPacketWriterStage();

    FFmpegVideoPacketWriterStage(const FFmpegVideoPacketWriterStage&) = delete;
    FFmpegVideoPacketWriterStage& operator=(const FFmpegVideoPacketWriterStage&) = delete;

    FFmpegVideoPacketWriterStage(FFmpegVideoPacketWriterStage&& other) noexcept;
    FFmpegVideoPacketWriterStage& operator=(FFmpegVideoPacketWriterStage&& other) noexcept;

    void reset();

    Status initialize(const Config& config);
    Status sendFrame(AVFrame* frame);
    Result<int> receiveAndWritePackets(const PacketWrittenCallback& onPacketWritten = {});

    bool isInitialized() const;

    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;

private:
    AVCodecContext* m_encoderCtx = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;

    int64_t m_packetCount = 0;
    int64_t m_lastWrittenDts = AV_NOPTS_VALUE;
    int64_t m_lastWrittenOutTimeMs = 0;
};

} // namespace media::ffmpeg
