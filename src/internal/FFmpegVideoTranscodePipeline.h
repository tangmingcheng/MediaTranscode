#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "internal/FFmpegVideoFilterGraph.h"
#include "internal/FFmpegVideoPipeline.h"

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

class TimelineNormalizer;

class FFmpegVideoTranscodePipeline {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        const TranscodeConfig* transcodeConfig = nullptr;
        AVStream* inputVideoStream = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        TimelineNormalizer* timeline = nullptr;
    };

    FFmpegVideoTranscodePipeline() = default;
    ~FFmpegVideoTranscodePipeline();

    FFmpegVideoTranscodePipeline(const FFmpegVideoTranscodePipeline&) = delete;
    FFmpegVideoTranscodePipeline& operator=(const FFmpegVideoTranscodePipeline&) = delete;

    FFmpegVideoTranscodePipeline(FFmpegVideoTranscodePipeline&& other) noexcept;
    FFmpegVideoTranscodePipeline& operator=(FFmpegVideoTranscodePipeline&& other) noexcept;

    void reset();

    bool initialize(const Config& config, std::string* error);
    bool processPacket(AVPacket* packet,
                       std::string* error,
                       const PacketWrittenCallback& onPacketWritten = {});

    bool flushDecoder(std::string* error,
                      const PacketWrittenCallback& onPacketWritten = {});
    bool flushFilterAndEncoder(std::string* error,
                               const PacketWrittenCallback& onPacketWritten = {});

    bool isInitialized() const;
    AVStream* outputStream() const;

    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;
    int64_t estimatedOutTimeMs() const;

private:
    bool openDecoder(std::string* error);
    bool openEncoderAndCreateOutputStream(std::string* error);
    bool initializeFilterGraph(std::string* error);
    bool initializePacketWriter(std::string* error);
    bool allocateFrames(std::string* error);

    bool drainDecoder(std::string* error,
                      const PacketWrittenCallback& onPacketWritten);
    bool processDecodedFrame(std::string* error,
                             const PacketWrittenCallback& onPacketWritten);
    bool drainFilterGraph(std::string* error,
                          const PacketWrittenCallback& onPacketWritten);
    bool writeEncodedPackets(AVFrame* frame,
                             std::string* error,
                             const PacketWrittenCallback& onPacketWritten);

    int64_t decodedFrameTimestamp() const;

private:
    TranscodeConfig m_config;

    AVStream* m_inputVideoStream = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    AVStream* m_outputVideoStream = nullptr;
    TimelineNormalizer* m_timeline = nullptr;

    AVCodecContext* m_decoderCtx = nullptr;
    AVCodecContext* m_encoderCtx = nullptr;

    VideoFilterGraph m_filterGraph;
    FFmpegVideoPipeline m_packetWriter;

    AVFrame* m_decodedFrame = nullptr;
    AVFrame* m_filteredFrame = nullptr;

    int64_t m_lastSubmittedPts = AV_NOPTS_VALUE;
    int64_t m_packetCount = 0;
    int64_t m_lastWrittenOutTimeMs = 0;

    int m_outputFps = 0;
    bool m_enableConstantFps = false;
};

} // namespace media::ffmpeg
