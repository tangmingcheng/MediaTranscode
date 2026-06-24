#include "media_transcode/MediaTranscode.h"

#include "internal/TranscodeTypes.h"
#include "local/FFmpegLocalFileTranscodeEngine.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace media {
namespace {

LocalVideoTranscodeProgress toPublicProgress(const ProgressInfo& info)
{
    LocalVideoTranscodeProgress progress;
    progress.frame = info.frame;
    progress.outTimeMs = info.outTimeMs;
    progress.speed = info.speed;
    progress.stage = info.raw;
    return progress;
}

TranscodeConfig toInternalConfig(const LocalVideoTranscodeConfig& config)
{
    TranscodeConfig internal;
    internal.inputUrl = config.inputPath;
    internal.outputUrl = config.outputPath;
    internal.width = config.width;
    internal.height = config.height;
    internal.fps = config.fps;

    internal.videoCodec = config.videoCodec;
    internal.videoBitrate.rateControl = config.rcMode;
    internal.videoBitrate.targetKbps = config.videoBitrateKbps;
    internal.videoBitrate.minKbps = config.minVideoBitrateKbps;
    internal.videoBitrate.maxKbps = config.maxVideoBitrateKbps;
    internal.videoBitrate.bufferSizeKbits = config.videoBufferSizeKbits;
    internal.videoBitrate.quality = config.quality;

    internal.videoEncode.speedPreset = config.speed;
    internal.videoEncode.gopSize = config.gopSize;
    internal.videoEncode.maxBFrames = config.maxBFrames;
    internal.videoEncode.tune = config.tune;
    internal.videoEncode.profile = config.profile;
    internal.videoEncode.level = config.level;

    internal.hardware.enabled = !config.disableHardware;
    internal.hardware.allowZeroCopyFallback = true;

    internal.audioEnabled = !config.noAudio;
    internal.audioCodec = config.audioCodec;
    internal.audioBitrateKbps = config.audioBitrateKbps;

    return internal;
}

ErrorInfo validateConfig(const LocalVideoTranscodeConfig& config)
{
    if (config.inputPath.empty()) {
        return ErrorInfo::invalidArgument("inputPath is empty");
    }

    if (config.outputPath.empty()) {
        return ErrorInfo::invalidArgument("outputPath is empty");
    }

    if (config.videoCodec == VideoCodec::Copy) {
        return ErrorInfo::invalidArgument("VideoCodec::Copy is not supported by local video transcode");
    }

    if ((config.width < 0) || (config.height < 0) || (config.fps < 0)) {
        return ErrorInfo::invalidArgument("width, height and fps must be greater than or equal to 0");
    }

    if ((config.videoBitrateKbps < 0) ||
        (config.minVideoBitrateKbps < 0) ||
        (config.maxVideoBitrateKbps < 0) ||
        (config.videoBufferSizeKbits < 0) ||
        (config.audioBitrateKbps < 0)) {
        return ErrorInfo::invalidArgument("bitrate values must be greater than or equal to 0");
    }

    if (config.quality < 0 || config.gopSize < 0 || config.maxBFrames < 0) {
        return ErrorInfo::invalidArgument("quality, gopSize and maxBFrames must be greater than or equal to 0");
    }

    if (config.minVideoBitrateKbps > 0 &&
        config.maxVideoBitrateKbps > 0 &&
        config.minVideoBitrateKbps > config.maxVideoBitrateKbps) {
        return ErrorInfo::invalidArgument("minVideoBitrateKbps must be less than or equal to maxVideoBitrateKbps");
    }

    return ErrorInfo::success();
}

ErrorInfo engineError(const FFmpegLocalFileTranscodeEngine& engine,
                      ErrorCode code,
                      const std::string& fallback)
{
    const std::string message = engine.lastError().empty()
        ? fallback
        : engine.lastError();
    return ErrorInfo::make(code, message);
}

} // namespace

struct LocalVideoTranscodeTask::Impl {
    mutable std::mutex mutex;
    FFmpegLocalFileTranscodeEngine engine;
    LocalVideoTranscodeReport report;
};

LocalVideoTranscodeTask::LocalVideoTranscodeTask(std::shared_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

LocalVideoTranscodeTask::~LocalVideoTranscodeTask() = default;

LocalVideoTranscodeTask::LocalVideoTranscodeTask(LocalVideoTranscodeTask&&) noexcept = default;

LocalVideoTranscodeTask& LocalVideoTranscodeTask::operator=(LocalVideoTranscodeTask&&) noexcept = default;

void LocalVideoTranscodeTask::stop()
{
    if (!m_impl) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->report.stopped = true;
    }

    m_impl->engine.stop();
}

Result<LocalVideoTranscodeReport> LocalVideoTranscodeTask::wait()
{
    if (!m_impl) {
        return Result<LocalVideoTranscodeReport>::failure(
            ErrorInfo::notInitialized("local video transcode task is empty")
        );
    }

    if (!m_impl->engine.wait()) {
        return Result<LocalVideoTranscodeReport>::failure(
            engineError(m_impl->engine, ErrorCode::InternalError, "local video transcode wait failed")
        );
    }

    const std::string lastError = m_impl->engine.lastError();
    if (!lastError.empty()) {
        return Result<LocalVideoTranscodeReport>::failure(
            ErrorInfo::ffmpegFailure(lastError)
        );
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->report.lastProgress.stage == "finished") {
        m_impl->report.completed = true;
        m_impl->report.stopped = false;
    }
    else if (m_impl->report.lastProgress.stage == "stopped") {
        m_impl->report.completed = false;
        m_impl->report.stopped = true;
    }
    return Result<LocalVideoTranscodeReport>::success(m_impl->report);
}

bool LocalVideoTranscodeTask::isRunning() const
{
    return m_impl && m_impl->engine.isRunning();
}

ErrorInfo LocalVideoTranscodeTask::lastError() const
{
    if (!m_impl) {
        return ErrorInfo::notInitialized("local video transcode task is empty");
    }

    const std::string lastError = m_impl->engine.lastError();
    if (lastError.empty()) {
        return ErrorInfo::success();
    }

    return ErrorInfo::ffmpegFailure(lastError);
}

LocalVideoTranscodeProgress LocalVideoTranscodeTask::lastProgress() const
{
    if (!m_impl) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->report.lastProgress;
}

Result<std::shared_ptr<LocalVideoTranscodeTask>> startLocalVideoTranscodeAsync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback)
{
    const ErrorInfo validation = validateConfig(config);
    if (!validation.ok()) {
        return Result<std::shared_ptr<LocalVideoTranscodeTask>>::failure(validation);
    }

    auto impl = std::make_shared<LocalVideoTranscodeTask::Impl>();
    impl->report.config = config;

    std::weak_ptr<LocalVideoTranscodeTask::Impl> weakImpl = impl;
    impl->engine.setProgressCallback(
        [weakImpl, progressCallback = std::move(progressCallback)](const ProgressInfo& info) {
            auto impl = weakImpl.lock();
            if (!impl) {
                return;
            }

            const LocalVideoTranscodeProgress progress = toPublicProgress(info);
            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->report.lastProgress = progress;
                if (progress.stage == "finished") {
                    impl->report.completed = true;
                    impl->report.stopped = false;
                }
                else if (progress.stage == "stopped") {
                    impl->report.completed = false;
                    impl->report.stopped = true;
                }
            }

            if (progressCallback) {
                progressCallback(progress);
            }
        }
    );

    if (!impl->engine.initialize(toInternalConfig(config))) {
        return Result<std::shared_ptr<LocalVideoTranscodeTask>>::failure(
            engineError(impl->engine, ErrorCode::InvalidArgument, "local video transcode initialize failed")
        );
    }

    if (!impl->engine.start()) {
        return Result<std::shared_ptr<LocalVideoTranscodeTask>>::failure(
            engineError(impl->engine, ErrorCode::InternalError, "local video transcode start failed")
        );
    }

    return Result<std::shared_ptr<LocalVideoTranscodeTask>>::success(
        std::shared_ptr<LocalVideoTranscodeTask>(new LocalVideoTranscodeTask(std::move(impl)))
    );
}

Result<LocalVideoTranscodeReport> startLocalVideoTranscodeSync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback)
{
    auto taskResult = startLocalVideoTranscodeAsync(config, std::move(progressCallback));
    if (!taskResult) {
        return Result<LocalVideoTranscodeReport>::failure(taskResult.error());
    }

    return taskResult.value()->wait();
}

} // namespace media
