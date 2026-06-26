#pragma once

#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegVideoDecoderStage.h"
#include "internal/FFmpegVideoEncoderStage.h"
#include "internal/FFmpegVideoFilterStage.h"
#include "internal/FFmpegVideoFrameRateStage.h"
#include "internal/FFmpegVideoFrameRoutingStrategy.h"
#include "internal/FFmpegVideoHardwareTransferStage.h"
#include "internal/FFmpegVideoInputMetadata.h"
#include "internal/FFmpegVideoPacketWriterStage.h"
#include "internal/FFmpegVideoTimestampStage.h"
#include "internal/TranscodeTypes.h"
#include "internal/output/EncodedPacketSink.h"
#include "internal/output/FFmpegMuxerPacketSink.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class TimelineNormalizer;

class FFmpegVideoProcessingPipeline {
public:
    using PacketWrittenCallback = std::function<void(int64_t packetCount, int64_t outTimeMs)>;

    struct Config {
        const TranscodeConfig* transcodeConfig = nullptr;
        const HardwarePipelinePlan* hardwarePlan = nullptr;
        AVStream* inputVideoStream = nullptr;
        AVFormatContext* outputFmtCtx = nullptr;
        TimelineNormalizer* timeline = nullptr;
        EncodedPacketSink* packetSink = nullptr;
    };

    FFmpegVideoProcessingPipeline() = default;
    ~FFmpegVideoProcessingPipeline();

    FFmpegVideoProcessingPipeline(const FFmpegVideoProcessingPipeline&) = delete;
    FFmpegVideoProcessingPipeline& operator=(const FFmpegVideoProcessingPipeline&) = delete;
    FFmpegVideoProcessingPipeline(FFmpegVideoProcessingPipeline&&) = delete;
    FFmpegVideoProcessingPipeline& operator=(FFmpegVideoProcessingPipeline&&) = delete;

    void reset();

    Status initialize(const Config& config);
    Status processPacket(AVPacket* packet,
                         const PacketWrittenCallback& onPacketWritten = {});

    Status flushDecoder(const PacketWrittenCallback& onPacketWritten = {});
    Status flushFilterAndEncoder(const PacketWrittenCallback& onPacketWritten = {});

    bool isInitialized() const;
    AVStream* outputStream() const;

    int64_t decodedFrameCount() const;
    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;
    int64_t estimatedOutTimeMs() const;

private:
    Status initializeTimestampStage(TimelineNormalizer* timeline);
    Status openDecoder();
    Status collectVideoInputMetadata();
    Status openEncoder();
    Status initializeFrameRoutingStrategy();
    Status initializeHardwareTransferStage();
    Status initializeFrameRateStage();
    Status initializeFilterStage();
    Status initializePacketWriter();
    Status allocateFrames();

    Status drainDecoder(const PacketWrittenCallback& onPacketWritten);
    Status processDecodedFrame(const PacketWrittenCallback& onPacketWritten);
    Status processHardwareFrameZeroCopy(const PacketWrittenCallback& onPacketWritten);
    Status processFrameThroughSoftwareFilter(AVFrame* frame,
                                             const PacketWrittenCallback& onPacketWritten);
    Status drainFrameRateStage(bool hardwareFrame,
                               const PacketWrittenCallback& onPacketWritten);
    Status sendFrameRateOutputToFilter(AVFrame* frame,
                                       bool hardwareFrame,
                                       const PacketWrittenCallback& onPacketWritten);
    Status drainFilterStage(const PacketWrittenCallback& onPacketWritten);
    Status writeEncodedPackets(AVFrame* frame,
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
    FFmpegVideoFrameRateStage m_frameRateStage;
    FFmpegVideoFilterStage m_filterStage;

    HardwarePipelinePlan m_hardwarePlan;
    bool m_hasHardwarePlan = false;

    EncodedPacketSink* m_packetSink = nullptr;
    FFmpegMuxerPacketSink m_defaultPacketSink;
    FFmpegVideoPacketWriterStage m_packetWriter;
    int64_t m_decodedFrameCount = 0;

    FramePtr m_decodedFrame;
    FramePtr m_frameRateFrame;
    FramePtr m_filteredFrame;
    FramePtr m_softwareTransferFrame;
};

} // namespace media::ffmpeg
