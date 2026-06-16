#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegVideoDecoderStage.h"
#include "internal/FFmpegVideoEncoderStage.h"
#include "internal/FFmpegVideoFilterStage.h"
#include "internal/FFmpegVideoFrameRoutingStrategy.h"
#include "internal/FFmpegVideoHardwareTransferStage.h"
#include "internal/FFmpegVideoInputMetadata.h"
#include "internal/FFmpegVideoPacketWriterStage.h"
#include "internal/FFmpegVideoTimestampStage.h"

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class TimelineNormalizer;

class FFmpegVideoTranscodePipeline {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        const TranscodeConfig* transcodeConfig = nullptr;
        const HardwarePipelinePlan* hardwarePlan = nullptr;
        AVStream* inputVideoStream = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        TimelineNormalizer* timeline = nullptr;
    };

    FFmpegVideoTranscodePipeline() = default;
    ~FFmpegVideoTranscodePipeline();

    FFmpegVideoTranscodePipeline(const FFmpegVideoTranscodePipeline&) = delete;
    FFmpegVideoTranscodePipeline& operator=(const FFmpegVideoTranscodePipeline&) = delete;
    FFmpegVideoTranscodePipeline(FFmpegVideoTranscodePipeline&&) = delete;
    FFmpegVideoTranscodePipeline& operator=(FFmpegVideoTranscodePipeline&&) = delete;

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
    bool initializeTimestampStage(TimelineNormalizer* timeline, std::string* error);
    bool openDecoder(std::string* error);
    bool collectVideoInputMetadata(std::string* error);
    bool openEncoder(std::string* error);
    bool initializeFrameRoutingStrategy(std::string* error);
    bool initializeHardwareTransferStage(std::string* error);
    bool initializeFilterStage(std::string* error);
    bool initializePacketWriter(std::string* error);
    bool allocateFrames(std::string* error);

    bool drainDecoder(std::string* error,
                      const PacketWrittenCallback& onPacketWritten);
    bool processDecodedFrame(std::string* error,
                             const PacketWrittenCallback& onPacketWritten);
    bool processHardwareFrameZeroCopy(std::string* error,
                                      const PacketWrittenCallback& onPacketWritten);
    bool processFrameThroughSoftwareFilter(AVFrame* frame,
                                           std::string* error,
                                           const PacketWrittenCallback& onPacketWritten);
    bool drainFilterStage(std::string* error,
                          const PacketWrittenCallback& onPacketWritten);
    bool writeEncodedPackets(AVFrame* frame,
                             std::string* error,
                             const PacketWrittenCallback& onPacketWritten);

    AVCodecContext* encoderContext() const;

private:
    TranscodeConfig m_config;

    AVStream* m_inputVideoStream = nullptr;
    AVFormatContext* m_outputFmtCtx = nullptr;
    FFmpegVideoInputMetadata m_inputMetadata;

    FFmpegVideoTimestampStage m_timestampStage;
    FFmpegVideoDecoderStage m_decoderStage;
    FFmpegVideoEncoderStage m_encoderStage;
    FFmpegVideoFrameRoutingStrategy m_frameRoutingStrategy;
    FFmpegVideoHardwareTransferStage m_hardwareTransferStage;
    FFmpegVideoFilterStage m_filterStage;

    HardwarePipelinePlan m_hardwarePlan;
    bool m_hasHardwarePlan = false;

    FFmpegVideoPacketWriterStage m_packetWriter;

    AVFrame* m_decodedFrame = nullptr;
    AVFrame* m_filteredFrame = nullptr;
    AVFrame* m_softwareTransferFrame = nullptr;
};

} // namespace media::ffmpeg
