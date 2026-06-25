#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"

#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegVideoTranscodePipeline.h"
#include "internal/input/FFmpegRealtimeInputSource.h"
#include "internal/output/FFmpegNullMuxer.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}

namespace media {
namespace {

bool validPort(int port)
{
    return port > 0 && port <= 65535;
}

bool validOptionalPort(int port)
{
    return port == 0 || validPort(port);
}

int64_t steadyNowMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

const char* stateName(RealtimeStreamState state)
{
    switch (state) {
    case RealtimeStreamState::Idle:
        return "idle";
    case RealtimeStreamState::Initialized:
        return "initialized";
    case RealtimeStreamState::Running:
        return "running";
    case RealtimeStreamState::StopRequested:
        return "stop-requested";
    case RealtimeStreamState::Stopped:
        return "stopped";
    case RealtimeStreamState::Failed:
        return "failed";
    default:
        return "unknown";
    }
}

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

class RunningStateGuard {
public:
    explicit RunningStateGuard(std::atomic_bool& running)
        : m_running(running)
    {
    }

    ~RunningStateGuard()
    {
        m_running.store(false);
    }

    RunningStateGuard(const RunningStateGuard&) = delete;
    RunningStateGuard& operator=(const RunningStateGuard&) = delete;

private:
    std::atomic_bool& m_running;
};

ffmpeg::FFmpegRealtimeInputSource::Config makeInputSourceConfig(
    const RealtimeCoreConfig& config)
{
    ffmpeg::FFmpegRealtimeInputSource::Config inputConfig;
    inputConfig.inputUrl = config.inputUrl;
    inputConfig.inputFormatHint = config.inputFormatHint;
    inputConfig.openTimeoutMs = config.openTimeoutMs;
    inputConfig.readTimeoutMs = config.readTimeoutMs;
    inputConfig.analyzeDurationUs = config.analyzeDurationUs;
    inputConfig.probeSizeBytes = config.probeSizeBytes;
    inputConfig.lowLatency = config.lowLatency;
    return inputConfig;
}

TranscodeConfig makeVideoPipelineConfig(const RealtimeCoreConfig& config)
{
    TranscodeConfig transcodeConfig;
    transcodeConfig.inputUrl = config.inputUrl;
    transcodeConfig.outputUrl = "p1-realtime-null";
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

} // namespace

FFmpegRealtimeStreamTranscodeEngine::FFmpegRealtimeStreamTranscodeEngine()
{
    avformat_network_init();
}

FFmpegRealtimeStreamTranscodeEngine::~FFmpegRealtimeStreamTranscodeEngine()
{
    requestStop();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

Status FFmpegRealtimeStreamTranscodeEngine::initialize(const RealtimeCoreConfig& config)
{
    if (m_running.load()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime stream transcode initialize failed: engine is running"));
    }

    const Status validation = validateConfig(config);
    if (!validation) {
        setLastError(validation.error());
        setState(RealtimeStreamState::Failed);
        return validation;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;
        m_stats = {};
        m_lastError = ErrorInfo::success();
        m_state = RealtimeStreamState::Initialized;
    }

    m_stopRequested.store(false);

    spdlog::info(
        "[REALTIME][CORE] initialized: input={}, rtp={}:{}, size={}x{}, fps={}, bitrate_kbps={}, hw={}, audio={}",
        config.inputUrl,
        config.rtpOutput.host,
        config.rtpOutput.rtpPort,
        config.width,
        config.height,
        config.fps,
        config.videoBitrateKbps,
        config.disableHardware ? "disabled" : "enabled",
        config.audioEnabled ? "enabled" : "disabled"
    );

    emitProgress("initialized");
    return Status::success();
}

Status FFmpegRealtimeStreamTranscodeEngine::start()
{
    if (m_running.load()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime stream transcode start failed: engine is already running"));
    }

    if (state() == RealtimeStreamState::Idle) {
        return Status::failure(ErrorInfo::notInitialized(
            "realtime stream transcode start failed: engine is not initialized"));
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    clearLastError();
    resetRuntimeState();
    m_stopRequested.store(false);
    m_running.store(true);

    try {
        m_workerThread = std::thread(&FFmpegRealtimeStreamTranscodeEngine::workerThread, this);
    }
    catch (const std::exception& e) {
        m_running.store(false);
        const auto error = ErrorInfo::internalError(
            std::string("realtime stream transcode start failed: create thread failed: ") + e.what());
        setLastError(error);
        setState(RealtimeStreamState::Failed);
        return Status::failure(error);
    }

    return Status::success();
}

Status FFmpegRealtimeStreamTranscodeEngine::run()
{
    if (m_running.load()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime stream transcode run failed: engine is already running"));
    }

    if (state() == RealtimeStreamState::Idle) {
        return Status::failure(ErrorInfo::notInitialized(
            "realtime stream transcode run failed: engine is not initialized"));
    }

    clearLastError();
    resetRuntimeState();
    m_stopRequested.store(false);
    m_running.store(true);

    RunningStateGuard runningGuard(m_running);
    const Status status = runLoop();
    if (!status) {
        setLastError(status.error());
        setState(RealtimeStreamState::Failed);
    }
    return status;
}

void FFmpegRealtimeStreamTranscodeEngine::requestStop()
{
    m_stopRequested.store(true);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_inputSource) {
            m_inputSource->requestInterrupt();
        }
    }

    if (m_running.load()) {
        setState(RealtimeStreamState::StopRequested);
        emitProgress("stop-requested");
    }
}

Status FFmpegRealtimeStreamTranscodeEngine::wait()
{
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    const ErrorInfo error = lastError();
    if (!error.ok()) {
        return Status::failure(error);
    }

    return Status::success();
}

bool FFmpegRealtimeStreamTranscodeEngine::isRunning() const
{
    return m_running.load();
}

bool FFmpegRealtimeStreamTranscodeEngine::stopRequested() const
{
    return m_stopRequested.load();
}

RealtimeStreamState FFmpegRealtimeStreamTranscodeEngine::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

RealtimeCoreStats FFmpegRealtimeStreamTranscodeEngine::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

ErrorInfo FFmpegRealtimeStreamTranscodeEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void FFmpegRealtimeStreamTranscodeEngine::setProgressCallback(ProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCallback = std::move(cb);
}

Status FFmpegRealtimeStreamTranscodeEngine::validateConfig(const RealtimeCoreConfig& config) const
{
    if (config.inputUrl.empty()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: inputUrl is empty"));
    }

    if (config.rtpOutput.host.empty()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: RTP output host is empty"));
    }

    if (!validPort(config.rtpOutput.rtpPort)) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: RTP port must be in range 1..65535"));
    }

    if (!validOptionalPort(config.rtpOutput.rtcpPort) ||
        !validOptionalPort(config.rtpOutput.localRtpPort) ||
        !validOptionalPort(config.rtpOutput.localRtcpPort)) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: optional RTP/RTCP ports must be 0 or in range 1..65535"));
    }

    if (config.rtpOutput.packetSize <= 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: RTP packetSize must be positive"));
    }

    if (config.width < 0 || config.height < 0 || config.fps < 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: width, height and fps must be greater than or equal to 0"));
    }

    if (config.videoCodec == VideoCodec::Copy) {
        return Status::failure(ErrorInfo::unsupported(
            "realtime core config is invalid: VideoCodec::Copy is not supported in P1 realtime transcode"));
    }

    if (config.videoBitrateKbps < 0 || config.gopSize < 0 || config.maxBFrames < 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: bitrate, gopSize and maxBFrames must be greater than or equal to 0"));
    }

    if (config.openTimeoutMs < 0 || config.readTimeoutMs < 0 ||
        config.analyzeDurationUs < 0 || config.probeSizeBytes < 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime core config is invalid: timeout, analyzeduration and probesize values must be greater than or equal to 0"));
    }

    return Status::success();
}

Status FFmpegRealtimeStreamTranscodeEngine::runLoop()
{
    setState(RealtimeStreamState::Running);
    emitProgress("running");

    spdlog::info("[REALTIME][CORE] run loop entered");

    RealtimeCoreConfig config;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        config = m_config;
        m_inputSource = std::make_unique<ffmpeg::FFmpegRealtimeInputSource>();
    }

    auto cleanupInputSource = [&]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_inputSource) {
            m_inputSource->close();
            m_inputSource.reset();
        }
    };

    ffmpeg::FFmpegRealtimeInputSource* inputSource = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        inputSource = m_inputSource.get();
    }

    if (!inputSource) {
        return Status::failure(ErrorInfo::internalError(
            "realtime run loop failed: input source is null"));
    }

    if (m_stopRequested.load()) {
        cleanupInputSource();
        setState(RealtimeStreamState::Stopped);
        emitProgress("stopped");
        return Status::success();
    }

    Status status = inputSource->open(makeInputSourceConfig(config));
    if (!status) {
        cleanupInputSource();
        return status;
    }
    emitProgress("input-opened");

    status = inputSource->findStreamInfo();
    if (!status) {
        cleanupInputSource();
        return status;
    }
    emitProgress("stream-info-ready");

    ffmpeg::TimelineNormalizer timeline;
    timeline.initStartFromFormat(
        inputSource->formatContext(),
        inputSource->videoStream(),
        nullptr
    );

    ffmpeg::FFmpegNullMuxer nullMuxer;
    status = nullMuxer.open();
    if (!status) {
        cleanupInputSource();
        return status;
    }

    const TranscodeConfig pipelineConfig = makeVideoPipelineConfig(config);
    const AVCodec* decoder = avcodec_find_decoder(inputSource->videoStream()->codecpar->codec_id);
    ffmpeg::HardwarePipelinePlan plan;
    const ffmpeg::HardwarePipelinePlan* executionPlan = nullptr;

    if (pipelineConfig.hardware.enabled) {
        plan = ffmpeg::FFmpegPipelinePlanner::planHardwarePipeline(
            pipelineConfig,
            decoder
        );
        if (plan.valid && plan.executionMode != ffmpeg::VideoExecutionMode::Cpu) {
            executionPlan = &plan;
            spdlog::info(
                "[REALTIME][PLAN] execution mode: {}: {}",
                executionModeName(plan.executionMode),
                plan.diagnostic
            );
        }
        else {
            if (!pipelineConfig.hardware.allowZeroCopyFallback) {
                cleanupInputSource();
                return Status::failure(ErrorInfo::hardwareUnavailable(
                    plan.diagnostic.empty()
                        ? "realtime hardware pipeline planning failed and fallback is disabled"
                        : plan.diagnostic));
            }
            spdlog::warn(
                "[REALTIME][PLAN] execution mode: cpu fallback: {}",
                plan.diagnostic.empty() ? "no hardware plan" : plan.diagnostic
            );
        }
    }
    else {
        plan.executionMode = ffmpeg::VideoExecutionMode::Cpu;
        plan.diagnostic = "hardware disabled by realtime config; using CPU pipeline";
        spdlog::warn("[REALTIME][PLAN] {}", plan.diagnostic);
    }

    ffmpeg::FFmpegVideoTranscodePipeline videoPipeline;
    {
        ffmpeg::FFmpegVideoTranscodePipeline::Config videoConfig;
        videoConfig.transcodeConfig = &pipelineConfig;
        videoConfig.hardwarePlan = executionPlan;
        videoConfig.inputVideoStream = inputSource->videoStream();
        videoConfig.outputFmtCtx = nullMuxer.context();
        videoConfig.timeline = &timeline;

        status = videoPipeline.initialize(videoConfig);
        if (!status) {
            cleanupInputSource();
            return status;
        }
    }

    status = nullMuxer.writeHeader();
    if (!status) {
        cleanupInputSource();
        return status;
    }
    emitProgress("video-pipeline-ready");

    ffmpeg::PacketPtr inputPacket = ffmpeg::makePacket();
    if (!inputPacket) {
        cleanupInputSource();
        return Status::failure(ErrorInfo::allocationFailed(
            "realtime run loop failed: av_packet_alloc input packet failed"));
    }

    auto updateVideoPipelineStats = [&](int64_t packetCount, int64_t outTimeMs) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
            m_stats.encodedVideoPacketCount = packetCount;
            m_stats.lastOutputTimeMs = outTimeMs;
        }

        if (packetCount == 1 || packetCount % 25 == 0) {
            emitProgress("encoding");
        }
    };

    auto finalizePipeline = [&](bool flush) -> Status {
        if (flush) {
            Status flushStatus = videoPipeline.flushDecoder(updateVideoPipelineStats);
            if (!flushStatus) {
                return flushStatus;
            }

            flushStatus = videoPipeline.flushFilterAndEncoder(updateVideoPipelineStats);
            if (!flushStatus) {
                return flushStatus;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
            m_stats.encodedVideoPacketCount = videoPipeline.packetCount();
            m_stats.lastOutputTimeMs = videoPipeline.lastWrittenOutTimeMs();
        }

        return nullMuxer.writeTrailer();
    };

    while (!m_stopRequested.load()) {
        auto readResult = inputSource->readPacket(inputPacket.get());
        if (!readResult) {
            cleanupInputSource();
            return Status::failure(readResult.error());
        }

        switch (readResult.value()) {
        case ffmpeg::RealtimeInputReadState::Packet: {
            const bool videoPacket = inputSource->isVideoPacket(inputPacket.get());
            const bool audioPacket = inputSource->isAudioPacket(inputPacket.get());
            int64_t videoPacketCount = 0;
            int64_t inputPacketCount = 0;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                ++m_stats.inputPacketCount;
                inputPacketCount = m_stats.inputPacketCount;
                m_stats.lastInputTimeMs = steadyNowMs();
                if (videoPacket) {
                    ++m_stats.inputVideoPacketCount;
                    videoPacketCount = m_stats.inputVideoPacketCount;
                }
            }

            if (videoPacket) {
                const Status processStatus = videoPipeline.processPacket(
                    inputPacket.get(),
                    updateVideoPipelineStats
                );
                av_packet_unref(inputPacket.get());

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stats.decodedVideoFrameCount = videoPipeline.decodedFrameCount();
                    m_stats.encodedVideoPacketCount = videoPipeline.packetCount();
                    m_stats.lastOutputTimeMs = videoPipeline.lastWrittenOutTimeMs();
                }

                if (!processStatus) {
                    cleanupInputSource();
                    return processStatus;
                }

                if (videoPacketCount == 1 || videoPacketCount % 100 == 0) {
                    emitProgress("input-video-packet");
                }
            }
            else {
                if (!audioPacket && (inputPacketCount == 1 || inputPacketCount % 200 == 0)) {
                    emitProgress("input-packet");
                }
                av_packet_unref(inputPacket.get());
            }
            break;
        }
        case ffmpeg::RealtimeInputReadState::TryAgain:
            av_packet_unref(inputPacket.get());
            break;
        case ffmpeg::RealtimeInputReadState::EndOfStream: {
            const Status finalizeStatus = finalizePipeline(true);
            cleanupInputSource();
            if (!finalizeStatus) {
                return finalizeStatus;
            }
            return Status::failure(ErrorInfo::ioFailure(
                "realtime input reached end of stream"));
        }
        case ffmpeg::RealtimeInputReadState::Interrupted:
            av_packet_unref(inputPacket.get());
            if (m_stopRequested.load()) {
                const Status finalizeStatus = finalizePipeline(false);
                cleanupInputSource();
                if (!finalizeStatus) {
                    return finalizeStatus;
                }
                setState(RealtimeStreamState::Stopped);
                emitProgress("stopped");
                return Status::success();
            }
            cleanupInputSource();
            return Status::failure(ErrorInfo::ioFailure(
                "realtime input read interrupted or timed out"));
        }
    }

    status = finalizePipeline(false);
    cleanupInputSource();
    if (!status) {
        return status;
    }

    setState(RealtimeStreamState::Stopped);
    emitProgress("stopped");
    return Status::success();
}

void FFmpegRealtimeStreamTranscodeEngine::workerThread()
{
    const Status status = runLoop();
    m_running.store(false);

    if (!status) {
        setLastError(status.error());
        setState(RealtimeStreamState::Failed);
        emitProgress("failed");
        return;
    }

    if (m_stopRequested.load()) {
        setState(RealtimeStreamState::Stopped);
        emitProgress("stopped");
    }
}

void FFmpegRealtimeStreamTranscodeEngine::resetRuntimeState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = {};
}

void FFmpegRealtimeStreamTranscodeEngine::clearLastError()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = ErrorInfo::success();
}

void FFmpegRealtimeStreamTranscodeEngine::setLastError(ErrorInfo error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = std::move(error);
}

void FFmpegRealtimeStreamTranscodeEngine::setState(RealtimeStreamState state)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = state;
    }

    spdlog::debug("[REALTIME][CORE] state={}", stateName(state));
}

void FFmpegRealtimeStreamTranscodeEngine::emitProgress(const std::string& stage)
{
    ProgressCallback callback;
    RealtimeCoreStats stats;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_progressCallback;
        stats = m_stats;
    }

    if (!callback) {
        return;
    }

    ProgressInfo info;
    info.frame = stats.encodedVideoPacketCount > 0
        ? stats.encodedVideoPacketCount
        : stats.inputVideoPacketCount;
    info.outTimeMs = std::max(stats.lastOutputTimeMs, stats.lastInputTimeMs);
    info.speed = 0.0;
    info.raw = stage;
    callback(info);
}

} // namespace media
