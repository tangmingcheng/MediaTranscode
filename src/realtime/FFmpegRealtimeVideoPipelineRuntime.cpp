#include "realtime/FFmpegRealtimeVideoPipelineRuntime.h"

#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegVideoTranscodePipeline.h"
#include "internal/output/FFmpegRtpMuxer.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media {

namespace {

const char* executionModeName(ffmpeg::VideoExecutionMode mode)
{
    switch (mode) {
    case ffmpeg::VideoExecutionMode::HardwareZeroCopy:
        return "hardware-zero-copy";
    case ffmpeg::VideoExecutionMode::HardwareDecodeSoftwareFilterHardwareEncode:
        return "hardware-decode-software-filter-hardware-encode";
    case ffmpeg::VideoExecutionMode::HardwareDecodeSoftwareFilterGenericEncode:
        return "hardware-decode-software-filter-generic-encode";
    case ffmpeg::VideoExecutionMode::Cpu:
    default:
        return "cpu";
    }
}

TranscodeConfig makeVideoPipelineConfig(const RealtimeCoreConfig& config)
{
    TranscodeConfig transcodeConfig;
    transcodeConfig.inputUrl = config.inputUrl;
    transcodeConfig.outputUrl = "p1-realtime-rtp";
    transcodeConfig.width = config.width;
    transcodeConfig.height = config.height;
    transcodeConfig.fps = config.fps;
    transcodeConfig.videoCodec = config.videoCodec;
    transcodeConfig.videoBitrate = config.videoBitrate;
    transcodeConfig.bitratePolicy = config.bitratePolicy;
    transcodeConfig.videoEncode = config.videoEncode;
    transcodeConfig.hardware = config.hardware;
    transcodeConfig.audioEnabled = false;
    return transcodeConfig;
}

ffmpeg::FFmpegRtpOutputConfig makeRtpOutputConfig(const RealtimeRtpOutputConfig& config)
{
    ffmpeg::FFmpegRtpOutputConfig outputConfig;
    outputConfig.host = config.host;
    outputConfig.rtpPort = config.rtpPort;
    outputConfig.rtcpPort = config.rtcpPort;
    outputConfig.localRtpPort = config.localRtpPort;
    outputConfig.localRtcpPort = config.localRtcpPort;
    outputConfig.packetSize = config.packetSize;
    outputConfig.sdpOutputPath = config.sdpOutputPath;
    return outputConfig;
}

} // namespace

struct FFmpegRealtimeVideoPipelineRuntime::Impl {
    Status initialize(const Config& config);
    Status processPacket(AVPacket* packet);
    Status finish(bool flush);
    void reset();
    RealtimeCoreStats stats() const;

    Status planPipeline();
    Status initializeMuxerAndPipeline();

    RealtimeCoreConfig realtimeConfig;
    TranscodeConfig pipelineConfig;

    AVFormatContext* inputFormatContext = nullptr;
    AVStream* inputVideoStream = nullptr;

    ffmpeg::TimelineNormalizer timeline;
    ffmpeg::HardwarePipelinePlan hardwarePlan;
    const ffmpeg::HardwarePipelinePlan* executionPlan = nullptr;
    ffmpeg::FFmpegRtpMuxer rtpMuxer;
    ffmpeg::FFmpegVideoTranscodePipeline videoPipeline;

    RealtimeCoreStats runtimeStats;
    bool initialized = false;
    bool finished = false;
};

FFmpegRealtimeVideoPipelineRuntime::FFmpegRealtimeVideoPipelineRuntime()
    : m_impl(std::make_unique<Impl>())
{
}

FFmpegRealtimeVideoPipelineRuntime::~FFmpegRealtimeVideoPipelineRuntime()
{
    reset();
}

Status FFmpegRealtimeVideoPipelineRuntime::initialize(const Config& config)
{
    return m_impl->initialize(config);
}

Status FFmpegRealtimeVideoPipelineRuntime::processPacket(AVPacket* packet)
{
    return m_impl->processPacket(packet);
}

Status FFmpegRealtimeVideoPipelineRuntime::finish(bool flush)
{
    return m_impl->finish(flush);
}

void FFmpegRealtimeVideoPipelineRuntime::reset()
{
    if (m_impl) {
        m_impl->reset();
    }
}

RealtimeCoreStats FFmpegRealtimeVideoPipelineRuntime::stats() const
{
    return m_impl ? m_impl->stats() : RealtimeCoreStats{};
}

Status FFmpegRealtimeVideoPipelineRuntime::Impl::initialize(const Config& config)
{
    reset();

    if (!config.realtimeConfig) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime video runtime initialize failed: realtimeConfig is null"));
    }

    if (!config.inputFormatContext) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime video runtime initialize failed: inputFormatContext is null"));
    }

    if (!config.inputVideoStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime video runtime initialize failed: inputVideoStream is null"));
    }

    realtimeConfig = *config.realtimeConfig;
    pipelineConfig = makeVideoPipelineConfig(realtimeConfig);
    inputFormatContext = config.inputFormatContext;
    inputVideoStream = config.inputVideoStream;

    timeline.initStartFromFormat(
        inputFormatContext,
        inputVideoStream,
        nullptr
    );

    Status status = planPipeline();
    if (!status) {
        reset();
        return status;
    }

    status = initializeMuxerAndPipeline();
    if (!status) {
        reset();
        return status;
    }

    initialized = true;
    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::Impl::planPipeline()
{
    executionPlan = nullptr;

    const AVCodec* decoder = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);

    if (pipelineConfig.hardware.enabled) {
        hardwarePlan = ffmpeg::FFmpegPipelinePlanner::planHardwarePipeline(
            pipelineConfig,
            decoder
        );

        if (hardwarePlan.valid &&
            hardwarePlan.executionMode != ffmpeg::VideoExecutionMode::Cpu) {
            executionPlan = &hardwarePlan;
            spdlog::info(
                "[REALTIME][PLAN] execution mode: {}: {}",
                executionModeName(hardwarePlan.executionMode),
                hardwarePlan.diagnostic
            );
            return Status::success();
        }

        if (!pipelineConfig.hardware.allowZeroCopyFallback) {
            return Status::failure(ErrorInfo::hardwareUnavailable(
                hardwarePlan.diagnostic.empty()
                    ? "realtime hardware pipeline planning failed and fallback is disabled"
                    : hardwarePlan.diagnostic));
        }

        spdlog::warn(
            "[REALTIME][PLAN] execution mode: cpu fallback: {}",
            hardwarePlan.diagnostic.empty() ? "no hardware plan" : hardwarePlan.diagnostic
        );
        return Status::success();
    }

    hardwarePlan.executionMode = ffmpeg::VideoExecutionMode::Cpu;
    hardwarePlan.diagnostic = "hardware disabled by realtime config; using CPU pipeline";
    spdlog::warn("[REALTIME][PLAN] {}", hardwarePlan.diagnostic);
    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::Impl::initializeMuxerAndPipeline()
{
    Status status = rtpMuxer.open(makeRtpOutputConfig(realtimeConfig.rtpOutput));
    if (!status) {
        return status;
    }

    ffmpeg::FFmpegVideoTranscodePipeline::Config videoConfig;
    videoConfig.transcodeConfig = &pipelineConfig;
    videoConfig.hardwarePlan = executionPlan;
    videoConfig.inputVideoStream = inputVideoStream;
    videoConfig.outputFmtCtx = rtpMuxer.context();
    videoConfig.timeline = &timeline;

    status = videoPipeline.initialize(videoConfig);
    if (!status) {
        return status;
    }

    status = rtpMuxer.writeHeader();
    if (!status) {
        return status;
    }

    status = rtpMuxer.writeSdp();
    if (!status) {
        return status;
    }

    spdlog::info(
        "[REALTIME][RTP] muxer ready: url={}, sdp={}",
        rtpMuxer.url(),
        realtimeConfig.rtpOutput.sdpOutputPath.empty()
            ? "disabled"
            : realtimeConfig.rtpOutput.sdpOutputPath
    );

    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::Impl::processPacket(AVPacket* packet)
{
    if (!initialized) {
        return Status::failure(ErrorInfo::notInitialized(
            "realtime video runtime processPacket failed: runtime is not initialized"));
    }

    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime video runtime processPacket failed: packet is null"));
    }

    auto onPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        runtimeStats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
        runtimeStats.encodedVideoPacketCount = packetCount;
        runtimeStats.muxedVideoPacketCount = packetCount;
        runtimeStats.lastOutputTimeMs = outTimeMs;
    };

    const Status status = videoPipeline.processPacket(packet, onPacketWritten);

    runtimeStats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
    runtimeStats.encodedVideoPacketCount = videoPipeline.packetCount();
    runtimeStats.muxedVideoPacketCount = videoPipeline.packetCount();
    runtimeStats.lastOutputTimeMs = videoPipeline.lastWrittenOutTimeMs();

    return status;
}

Status FFmpegRealtimeVideoPipelineRuntime::Impl::finish(bool flush)
{
    if (!initialized || finished) {
        return Status::success();
    }

    auto onPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        runtimeStats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
        runtimeStats.encodedVideoPacketCount = packetCount;
        runtimeStats.muxedVideoPacketCount = packetCount;
        runtimeStats.lastOutputTimeMs = outTimeMs;
    };

    if (flush) {
        Status status = videoPipeline.flushDecoder(onPacketWritten);
        if (!status) {
            return status;
        }

        status = videoPipeline.flushFilterAndEncoder(onPacketWritten);
        if (!status) {
            return status;
        }
    }

    runtimeStats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
    runtimeStats.encodedVideoPacketCount = videoPipeline.packetCount();
    runtimeStats.muxedVideoPacketCount = videoPipeline.packetCount();
    runtimeStats.lastOutputTimeMs = videoPipeline.lastWrittenOutTimeMs();

    Status trailerStatus = rtpMuxer.writeTrailer();
    finished = true;
    return trailerStatus;
}

void FFmpegRealtimeVideoPipelineRuntime::Impl::reset()
{
    videoPipeline.reset();
    rtpMuxer.reset();
    timeline.reset();

    realtimeConfig = RealtimeCoreConfig{};
    pipelineConfig = TranscodeConfig{};
    inputFormatContext = nullptr;
    inputVideoStream = nullptr;
    hardwarePlan = ffmpeg::HardwarePipelinePlan{};
    executionPlan = nullptr;
    runtimeStats = RealtimeCoreStats{};
    initialized = false;
    finished = false;
}

RealtimeCoreStats FFmpegRealtimeVideoPipelineRuntime::Impl::stats() const
{
    return runtimeStats;
}

} // namespace media
