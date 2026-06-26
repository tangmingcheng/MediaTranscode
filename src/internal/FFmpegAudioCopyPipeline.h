#pragma once

#include "internal/FFmpegAudioPacketWriter.h"
#include "internal/FFmpegAudioPipelineStrategy.h"

namespace media::ffmpeg {

class FFmpegAudioCopyPipeline final : public IFFmpegAudioPipelineStrategy {
public:
    FFmpegAudioCopyPipeline() = default;
    ~FFmpegAudioCopyPipeline() override = default;

    FFmpegAudioCopyPipeline(const FFmpegAudioCopyPipeline&) = delete;
    FFmpegAudioCopyPipeline& operator=(const FFmpegAudioCopyPipeline&) = delete;

    void reset() override;

    Status initialize(const FFmpegAudioPipelineConfig& config) override;
    Status processPacket(AVPacket* packet,
                         const FFmpegAudioPacketWrittenCallback& onPacketWritten) override;
    Status flush(const FFmpegAudioPacketWrittenCallback& onPacketWritten) override;

    bool isInitialized() const override;
    FFmpegAudioPipelineMode mode() const override;
    AVStream* outputStream() const override;
    FFmpegAudioPacketProgress progress() const override;

private:
    Status normalizePacketTimestamp(AVPacket* packet) const;

private:
    AVStream* m_inputAudioStream = nullptr;
    AudioOutputStreamProvider* m_outputStreamProvider = nullptr;
    PacketOutputNode* m_outputNode = nullptr;
    AVStream* m_outputAudioStream = nullptr;
    TimelineNormalizer* m_timeline = nullptr;

    FFmpegAudioPacketWriter m_packetWriter;
};

} // namespace media::ffmpeg
