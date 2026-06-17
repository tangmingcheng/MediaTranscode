#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "media_transcode/Result.h"
#include "internal/FFmpegRAII.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

namespace media::ffmpeg {

class FFmpegAudioFifo;
class TimelineNormalizer;

class FFmpegAudioPipeline {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        /*
         * FFmpegAudioPipeline owns audio decoder / encoder / swr / fifo resources.
         * inputAudioStream, outputFmtCtx and timeline are borrowed from FFmpegTranscoder.
         */
        AudioMode mode = AudioMode::None;
        AudioCodec codec = AudioCodec::AAC;
        AVStream* inputAudioStream = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        TimelineNormalizer* timeline = nullptr;
        int audioBitrateKbps = 128;
    };

    FFmpegAudioPipeline();
    ~FFmpegAudioPipeline();

    FFmpegAudioPipeline(const FFmpegAudioPipeline&) = delete;
    FFmpegAudioPipeline& operator=(const FFmpegAudioPipeline&) = delete;

    FFmpegAudioPipeline(FFmpegAudioPipeline&& other) noexcept;
    FFmpegAudioPipeline& operator=(FFmpegAudioPipeline&& other) noexcept;

    void reset();

    Status initialize(const Config& config);
    Status processPacket(AVPacket* packet,
                         const PacketWrittenCallback& onPacketWritten = {});
    Status flush(const PacketWrittenCallback& onPacketWritten = {});

    /*
     * 保留底层音频编码写包接口，方便后续实时帧输入复用。
     */
    Status sendFrame(AVFrame* frame);
    Result<int> receiveAndWritePackets(const PacketWrittenCallback& onPacketWritten = {});

    bool isInitialized() const;
    AudioMode mode() const;
    AVStream* outputStream() const;

    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;

private:
    Status initializeCopy();
    Status initializeEncode();
    Status initializeResamplerAndFifo();

    Status writeCopyPacket(AVPacket* packet,
                           const PacketWrittenCallback& onPacketWritten);
    Status normalizeCopyPacketTimestamp(AVPacket* packet) const;

    Status sendPacketToDecoder(AVPacket* packet,
                               const PacketWrittenCallback& onPacketWritten);
    Status drainDecoder(const PacketWrittenCallback& onPacketWritten);
    Status pushDecodedFrameToFifo(const PacketWrittenCallback& onPacketWritten);
    Status ensureInitialAudioPts();
    Status encodeFifo(bool flushAll,
                      const PacketWrittenCallback& onPacketWritten);
    Status flushResampler();
    bool updateProgressFromPacket(const AVPacket* packet);

private:
    AudioMode m_mode = AudioMode::None;
    AudioCodec m_codec = AudioCodec::AAC;

    AVStream* m_inputAudioStream = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    AVStream* m_outputAudioStream = nullptr;
    TimelineNormalizer* m_timeline = nullptr;

    CodecContextPtr m_decoderCtx;
    CodecContextPtr m_encoderCtx;
    SwrContextPtr m_swrCtx;
    std::unique_ptr<FFmpegAudioFifo> m_fifo;
    FramePtr m_decodedFrame;

    int64_t m_packetCount = 0;
    int64_t m_lastWrittenDts = AV_NOPTS_VALUE;
    int64_t m_lastWrittenOutTimeMs = 0;
    int64_t m_nextAudioPts = AV_NOPTS_VALUE;
    int m_audioBitrateKbps = 128;
};

} // namespace media::ffmpeg
