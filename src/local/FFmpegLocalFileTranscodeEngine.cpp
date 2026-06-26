#include "local/FFmpegLocalFileTranscodeEngine.h"

#include "local/FFmpegLocalTranscodeRuntime.h"

#include <exception>
#include <mutex>
#include <string>
#include <utility>

extern "C" {
#include <libavformat/avformat.h>
}

namespace media {
namespace {

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

FFmpegLocalFileTranscodeEngine::FFmpegLocalFileTranscodeEngine()
{
    avformat_network_init();
}

FFmpegLocalFileTranscodeEngine::~FFmpegLocalFileTranscodeEngine()
{
    stop();
}

bool FFmpegLocalFileTranscodeEngine::initialize(const TranscodeConfig& config)
{
    if (m_running.load()) {
        setLastError("initialize failed: local file transcode engine is running");
        return false;
    }

    if (config.inputUrl.empty()) {
        setLastError("initialize failed: input path is empty");
        return false;
    }

    if (config.outputUrl.empty()) {
        setLastError("initialize failed: output path is empty");
        return false;
    }

    if (config.videoCodec == VideoCodec::Copy) {
        setLastError("initialize failed: VideoCodec::Copy is not implemented in local video transcode API");
        return false;
    }

    m_config = config;
    clearLastError();

    return true;
}

bool FFmpegLocalFileTranscodeEngine::start()
{
    if (m_running.load()) {
        setLastError("start failed: local file transcode engine is already running");
        return false;
    }

    if (m_config.inputUrl.empty() || m_config.outputUrl.empty()) {
        setLastError("start failed: local file transcode engine is not initialized");
        return false;
    }

    /*
     * 防止同一个对象二次 start 时，之前线程虽然结束但还没有 join。
     */
    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    clearLastError();

    m_stopRequested.store(false);
    m_running.store(true);

    try {
        m_transcodeThread = std::thread(&FFmpegLocalFileTranscodeEngine::transcodeThread, this);
    }
    catch (const std::exception& e) {
        m_running.store(false);
        setLastError(std::string("start failed: create thread failed: ") + e.what());
        return false;
    }

    return true;
}

void FFmpegLocalFileTranscodeEngine::stop()
{
    m_stopRequested.store(true);

    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    m_running.store(false);
}

bool FFmpegLocalFileTranscodeEngine::wait()
{
    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    return !m_running.load();
}

bool FFmpegLocalFileTranscodeEngine::isRunning() const
{
    return m_running.load();
}

std::string FFmpegLocalFileTranscodeEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void FFmpegLocalFileTranscodeEngine::setProgressCallback(ProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCallback = std::move(cb);
}

void FFmpegLocalFileTranscodeEngine::transcodeThread()
{
    RunningStateGuard runningGuard(m_running);

    ffmpeg::FFmpegLocalTranscodeRuntime::Config runtimeConfig;
    runtimeConfig.transcodeConfig = m_config;
    runtimeConfig.stopRequested = &m_stopRequested;
    runtimeConfig.progressCallback = [this](const ProgressInfo& info) {
        ProgressCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = m_progressCallback;
        }

        if (callback) {
            callback(info);
        }
    };

    ffmpeg::FFmpegLocalTranscodeRuntime runtime;
    const Status status = runtime.run(std::move(runtimeConfig));
    if (!status) {
        setLastError(status.error().message);
    }
}

void FFmpegLocalFileTranscodeEngine::setLastError(const std::string& error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = error;
}

void FFmpegLocalFileTranscodeEngine::clearLastError()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError.clear();
}

} // namespace media
