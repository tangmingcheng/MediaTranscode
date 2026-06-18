#pragma once

#include "internal/FFmpegAudioPipelineTypes.h"

#include <memory>

namespace media::ffmpeg {

class FFmpegAudioPipeline {
public:
    using PacketWrittenCallback = FFmpegAudioPacketWrittenCallback;
    using Config = FFmpegAudioPipelineConfig;

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
     * 只有 EncodeSelected 策略支持这两个接口。
     */
    Status sendFrame(AVFrame* frame);
    Result<int> receiveAndWritePackets(const PacketWrittenCallback& onPacketWritten = {});

    bool isInitialized() const;
    AudioMode mode() const;
    AVStream* outputStream() const;

    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg
