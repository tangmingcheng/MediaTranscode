#pragma once

#include "internal/FFmpegAudioPipelineTypes.h"

namespace media::ffmpeg {

class IFFmpegAudioPipelineStrategy {
public:
    virtual ~IFFmpegAudioPipelineStrategy() = default;

    virtual void reset() = 0;

    virtual Status initialize(const FFmpegAudioPipelineConfig& config) = 0;
    virtual Status processPacket(AVPacket* packet,
                                 const FFmpegAudioPacketWrittenCallback& onPacketWritten) = 0;
    virtual Status flush(const FFmpegAudioPacketWrittenCallback& onPacketWritten) = 0;

    virtual Status sendFrame(AVFrame* frame);
    virtual Result<int> receiveAndWritePackets(
        const FFmpegAudioPacketWrittenCallback& onPacketWritten);

    virtual bool isInitialized() const = 0;
    virtual FFmpegAudioPipelineMode mode() const = 0;
    virtual AVStream* outputStream() const = 0;
    virtual FFmpegAudioPacketProgress progress() const = 0;
};

} // namespace media::ffmpeg
