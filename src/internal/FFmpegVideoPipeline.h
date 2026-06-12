#pragma once

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegVideoPipeline {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        /*
         * FFmpegVideoPipeline does not own these contexts.
         * The caller is responsible for allocating and freeing them.
         */
        AVCodecContext* encoderCtx = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        AVStream* outputVideoStream = nullptr;
    };

    FFmpegVideoPipeline() = default;
    ~FFmpegVideoPipeline();

    FFmpegVideoPipeline(const FFmpegVideoPipeline&) = delete;
    FFmpegVideoPipeline& operator=(const FFmpegVideoPipeline&) = delete;

    FFmpegVideoPipeline(FFmpegVideoPipeline&& other) noexcept;
    FFmpegVideoPipeline& operator=(FFmpegVideoPipeline&& other) noexcept;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool sendFrame(AVFrame* frame, std::string* error);
    int receiveAndWritePackets(std::string* error,
                               const PacketWrittenCallback& onPacketWritten = {});

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
