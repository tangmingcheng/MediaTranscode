#include "realtime/FFmpegRealtimeStreamTranscodeEngine.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <exception>
#include <utility>

extern "C" {
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

    if (m_stopRequested.load()) {
        setState(RealtimeStreamState::Stopped);
        emitProgress("stopped");
        return Status::success();
    }

    /*
     * P1-Core-1 intentionally stops here. The next steps should connect the
     * realtime input source, existing video transcode pipeline, and RTP muxer.
     */
    const auto error = ErrorInfo::unsupported(
        "P1-Core-1 skeleton is ready; realtime input source is not implemented yet");

    spdlog::warn("[REALTIME][CORE] {}", error.message);
    emitProgress("skeleton-ready");
    return Status::failure(error);
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
    info.frame = stats.encodedVideoPacketCount;
    info.outTimeMs = std::max(stats.lastOutputTimeMs, stats.lastInputTimeMs);
    info.speed = 0.0;
    info.raw = stage;
    callback(info);
}

} // namespace media
