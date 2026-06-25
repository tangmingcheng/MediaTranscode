#include "realtime/FFmpegRealtimeVideoPipelineRuntime.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavcodec/avcodec.h>
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
    transcodeConfig.videoBitrate.rateControl = config.rcMode;
    transcodeConfig.videoBitrate.targetKbps = config.videoBitrateKbps;
    transcodeConfig.videoEncode.speedPreset = config.speed;
    transcodeConfig.videoEncode.gopSize = config.gopSize;
    transcodeConfig.videoEncode.maxBFrames = config.maxBFrames;
    transcodeConfig.videoEncode.tune = config.tune;
    transcodeConfig.videoEncode.profile = config.profile;
    transcodeConfig.videoEncode.level = config.level;
    transcodeConfig.audioEnabled = false;
    transcodeConfig.hardware.enabled = !config.disableHardware;
    transcodeConfig.hardware.allowZeroCopyFallback = config.allowHardwareFallback;
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

FFmpegRealtimeVideoPipelineRuntime::~FFmpegRealtimeVideoPipelineRuntime()
{
    reset();
}

Status FFmpegRealtimeVideoPipelineRuntime::initialize(const Config& config)
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

    m_realtimeConfig = *config.realtimeConfig;
    m_pipelineConfig = makeVideoPipelineConfig(m_realtimeConfig);
    m_inputFormatContext = config.inputFormatContext;
    m_inputVideoStream = config.inputVideoStream;

    m_timeline.initStartFromFormat(
        m_inputFormatContext,
        m_inputVideoStream,
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

    m_initialized = true;
    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::planPipeline()
{
    m_executionPlan = nullptr;

    const AVCodec* decoder = avcodec_find_decoder(m_inputVideoStream->codecpar->codec_id);

    if (m_pipelineConfig.hardware.enabled) {
        m_hardwarePlan = ffmpeg::FFmpegPipelinePlanner::planHardwarePipeline(
            m_pipelineConfig,
            decoder
        );

        if (m_hardwarePlan.valid &&
            m_hardwarePlan.executionMode != ffmpeg::VideoExecutionMode::Cpu) {
            m_executionPlan = &m_hardwarePlan;
            spdlog::info(
                "[REALTIME][PLAN] execution mode: {}: {}",
                executionModeName(m_hardwarePlan.executionMode),
                m_hardwarePlan.diagnostic
            );
            return Status::success();
        }

        if (!m_pipelineConfig.hardware.allowZeroCopyFallback) {
            return Status::failure(ErrorInfo::hardwareUnavailable(
                m_hardwarePlan.diagnostic.empty()
                    ? "realtime hardware pipeline planning failed and fallback is disabled"
                    : m_hardwarePlan.diagnostic));
        }

        spdlog::warn(
            "[REALTIME][PLAN] execution mode: cpu fallback: {}",
            m_hardwarePlan.diagnostic.empty() ? "no hardware plan" : m_hardwarePlan.diagnostic
        );
        return Status::success();
    }

    m_hardwarePlan.executionMode = ffmpeg::VideoExecutionMode::Cpu;
    m_hardwarePlan.diagnostic = "hardware disabled by realtime config; using CPU pipeline";
    spdlog::warn("[REALTIME][PLAN] {}", m_hardwarePlan.diagnostic);
    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::initializeMuxerAndPipeline()
{
    Status status = m_rtpMuxer.open(makeRtpOutputConfig(m_realtimeConfig.rtpOutput));
    if (!status) {
        return status;
    }

    ffmpeg::FFmpegVideoTranscodePipeline::Config videoConfig;
    videoConfig.transcodeConfig = &m_pipelineConfig;
    videoConfig.hardwarePlan = m_executionPlan;
    videoConfig.inputVideoStream = m_inputVideoStream;
    videoConfig.outputFmtCtx = m_rtpMuxer.context();
    videoConfig.timeline = &m_timeline;

    status = m_videoPipeline.initialize(videoConfig);
    if (!status) {
        return status;
    }

    status = m_rtpMuxer.writeHeader();
    if (!status) {
        return status;
    }

    status = m_rtpMuxer.writeSdp();
    if (!status) {
        return status;
    }

    spdlog::info(
        "[REALTIME][RTP] muxer ready: url={}, sdp={}",
        m_rtpMuxer.url(),
        m_realtimeConfig.rtpOutput.sdpOutputPath.empty()
            ? "disabled"
            : m_realtimeConfig.rtpOutput.sdpOutputPath
    );

    return Status::success();
}

Status FFmpegRealtimeVideoPipelineRuntime::processPacket(AVPacket* packet)
{
    if (!m_initialized) {
        return Status::failure(ErrorInfo::notInitialized(
            "realtime video runtime processPacket failed: runtime is not initialized"));
    }

    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime video runtime processPacket failed: packet is null"));
    }

    auto onPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        m_stats.decodedVideoFrameCount = m_videoPipeline.decodedFrameCount();
        m_stats.encodedVideoPacketCount = packetCount;
        m_stats.writtenRtpPacketCount = packetCount;
        m_stats.lastOutputTimeMs = outTimeMs;
    };

    const Status status = m_videoPipeline.processPacket(packet, onPacketWritten);

    m_stats.decodedVideoFrameCount = m_videoPipeline.decodedFrameCount();
    m_stats.encodedVideoPacketCount = m_videoPipeline.packetCount();
    m_stats.writtenRtpPacketCount = m_videoPipeline.packetCount();
    m_stats.lastOutputTimeMs = m_videoPipeline.lastWrittenOutTimeMs();

    return status;
}

Status FFmpegRealtimeVideoPipelineRuntime::finish(bool flush)
{
    if (!m_initialized || m_finished) {
        return Status::success();
    }

    auto onPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        m_stats.decodedVideoFrameCount = m_videoPipeline.decodedFrameCount();
        m_stats.encodedVideoPacketCount = packetCount;
        m_stats.writtenRtpPacketCount = packetCount;
        m_stats.lastOutputTimeMs = outTimeMs;
    };

    if (flush) {
        Status status = m_videoPipeline.flushDecoder(onPacketWritten);
        if (!status) {
            return status;
        }

        status = m_videoPipeline.flushFilterAndEncoder(onPacketWritten);
        if (!status) {
            return status;
        }
    }

    m_stats.decodedVideoFrameCount = m_videoPipeline.decodedFrameCount();
    m_stats.encodedVideoPacketCount = m_videoPipeline.packetCount();
    m_stats.writtenRtpPacketCount = m_videoPipeline.packetCount();
    m_stats.lastOutputTimeMs = m_videoPipeline.lastWrittenOutTimeMs();

    Status trailerStatus = m_rtpMuxer.writeTrailer();
    m_finished = true;
    return trailerStatus;
}

void FFmpegRealtimeVideoPipelineRuntime::reset()
{
    m_videoPipeline.reset();
    m_rtpMuxer.reset();
    m_timeline.reset();

    m_realtimeConfig = RealtimeCoreConfig{};
    m_pipelineConfig = TranscodeConfig{};
    m_inputFormatContext = nullptr;
    m_inputVideoStream = nullptr;
    m_hardwarePlan = ffmpeg::HardwarePipelinePlan{};
    m_executionPlan = nullptr;
    m_stats = RealtimeCoreStats{};
    m_initialized = false;
    m_finished = false;
}

RealtimeCoreStats FFmpegRealtimeVideoPipelineRuntime::stats() const
{
    return m_stats;
}

} // namespace media
