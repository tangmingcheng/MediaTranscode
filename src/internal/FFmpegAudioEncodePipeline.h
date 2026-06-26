#pragma once

#include "internal/FFmpegAudioAdapter.h"
#include "internal/FFmpegAudioPacketWriter.h"
#include "internal/FFmpegAudioPipelineStrategy.h"
#include "internal/FFmpegAudioProcessDiagnostics.h"
#include "internal/FFmpegRAII.h"

#include <memory>

namespace media::ffmpeg {

class FFmpegAudioFifo;

class FFmpegAudioEncodePipeline final : public IFFmpegAudioPipelineStrategy {
public:
    FFmpegAudioEncodePipeline();
    ~FFmpegAudioEncodePipeline() override;

    FFmpegAudioEncodePipeline(const FFmpegAudioEncodePipeline&) = delete;
    FFmpegAudioEncodePipeline& operator=(const FFmpegAudioEncodePipeline&) = delete;

    void reset() override;

    Status initialize(const FFmpegAudioPipelineConfig& config) override;
    Status processPacket(AVPacket* packet,
                         const FFmpegAudioPacketWrittenCallback& onPacketWritten) override;
    Status flush(const FFmpegAudioPacketWrittenCallback& onPacketWritten) override;

    Status sendFrame(AVFrame* frame) override;
    Result<int> receiveAndWritePackets(
        const FFmpegAudioPacketWrittenCallback& onPacketWritten) override;

    bool isInitialized() const override;
    FFmpegAudioPipelineMode mode() const override;
    AVStream* outputStream() const override;
    FFmpegAudioPacketProgress progress() const override;

private:
    Status initializeDecoder();
    Status initializeEncoder();
    Status initializeOutputStream();
    Status initializeResamplerAndFifo();

    Status sendPacketToDecoder(AVPacket* packet,
                               const FFmpegAudioPacketWrittenCallback& onPacketWritten);
    Status drainDecoder(const FFmpegAudioPacketWrittenCallback& onPacketWritten);
    Status pushDecodedFrameToFifo(const FFmpegAudioPacketWrittenCallback& onPacketWritten);
    Status ensureInitialAudioPts();
    Status encodeFifo(bool flushAll,
                      const FFmpegAudioPacketWrittenCallback& onPacketWritten);
    Status flushResampler();

private:
    AudioCodec m_codec = AudioCodec::AAC;

    AVStream* m_inputAudioStream = nullptr;
    AudioOutputStreamProvider* m_outputStreamProvider = nullptr;
    PacketOutputNode* m_outputNode = nullptr;
    AVStream* m_outputAudioStream = nullptr;
    TimelineNormalizer* m_timeline = nullptr;

    CodecContextPtr m_decoderCtx;
    CodecContextPtr m_encoderCtx;
    SwrContextPtr m_swrCtx;
    std::unique_ptr<FFmpegAudioFifo> m_fifo;
    FramePtr m_decodedFrame;

    FFmpegAudioPacketWriter m_packetWriter;
    FFmpegAudioProcessDiagnostics m_processDiagnostics;

    int64_t m_nextAudioPts = -9223372036854775807LL - 1LL;
    int m_audioBitrateKbps = 128;
};

} // namespace media::ffmpeg
