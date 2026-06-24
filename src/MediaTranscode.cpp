#include "media_transcode/LocalVideoTranscode.h"

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

ErrorInfo invalidJobError()
{
    return ErrorInfo::notInitialized("local video transcode job is empty");
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

struct LocalVideoTranscodeJob {
    ~LocalVideoTranscodeJob()
    {
        engine.stop();
    }

    mutable std::mutex mutex;
    FFmpegLocalFileTranscodeEngine engine;
    LocalVideoTranscodeReport report;
};

Result<LocalVideoTranscodeJobHandle> startLocalVideoTranscodeAsync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback)
{
    const ErrorInfo validation = validateConfig(config);
    if (!validation.ok()) {
        return Result<LocalVideoTranscodeJobHandle>::failure(validation);
    }

    auto job = std::make_shared<LocalVideoTranscodeJob>();
    job->report.config = config;

    std::weak_ptr<LocalVideoTranscodeJob> weakJob = job;
    job->engine.setProgressCallback(
        [weakJob, progressCallback = std::move(progressCallback)](const ProgressInfo& info) {
            auto job = weakJob.lock();
            if (!job) {
                return;
            }

            const LocalVideoTranscodeProgress progress = toPublicProgress(info);
            {
                std::lock_guard<std::mutex> lock(job->mutex);
                job->report.lastProgress = progress;
                if (progress.stage == "finished") {
                    job->report.completed = true;
                    job->report.stopped = false;
                }
                else if (progress.stage == "stopped") {
                    job->report.completed = false;
                    job->report.stopped = true;
                }
            }

            if (progressCallback) {
                progressCallback(progress);
            }
        }
    );

    if (!job->engine.initialize(toInternalConfig(config))) {
        return Result<LocalVideoTranscodeJobHandle>::failure(
            engineError(job->engine, ErrorCode::InvalidArgument, "local video transcode initialize failed")
        );
    }

    if (!job->engine.start()) {
        return Result<LocalVideoTranscodeJobHandle>::failure(
            engineError(job->engine, ErrorCode::InternalError, "local video transcode start failed")
        );
    }

    return Result<LocalVideoTranscodeJobHandle>::success(std::move(job));
}

Result<LocalVideoTranscodeReport> startLocalVideoTranscodeSync(
    const LocalVideoTranscodeConfig& config,
    LocalVideoTranscodeProgressCallback progressCallback)
{
    auto jobResult = startLocalVideoTranscodeAsync(config, std::move(progressCallback));
    if (!jobResult) {
        return Result<LocalVideoTranscodeReport>::failure(jobResult.error());
    }

    return waitLocalVideoTranscode(jobResult.value());
}

Result<void> stopLocalVideoTranscode(const LocalVideoTranscodeJobHandle& job)
{
    if (!job) {
        return Result<void>::failure(invalidJobError());
    }

    {
        std::lock_guard<std::mutex> lock(job->mutex);
        job->report.stopped = true;
    }

    job->engine.stop();
    return Result<void>::success();
}

Result<LocalVideoTranscodeReport> waitLocalVideoTranscode(const LocalVideoTranscodeJobHandle& job)
{
    if (!job) {
        return Result<LocalVideoTranscodeReport>::failure(invalidJobError());
    }

    if (!job->engine.wait()) {
        return Result<LocalVideoTranscodeReport>::failure(
            engineError(job->engine, ErrorCode::InternalError, "local video transcode wait failed")
        );
    }

    const std::string lastError = job->engine.lastError();
    if (!lastError.empty()) {
        return Result<LocalVideoTranscodeReport>::failure(
            ErrorInfo::ffmpegFailure(lastError)
        );
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->report.lastProgress.stage == "finished") {
        job->report.completed = true;
        job->report.stopped = false;
    }
    else if (job->report.lastProgress.stage == "stopped") {
        job->report.completed = false;
        job->report.stopped = true;
    }
    return Result<LocalVideoTranscodeReport>::success(job->report);
}

bool isLocalVideoTranscodeRunning(const LocalVideoTranscodeJobHandle& job)
{
    return job && job->engine.isRunning();
}

ErrorInfo getLocalVideoTranscodeLastError(const LocalVideoTranscodeJobHandle& job)
{
    if (!job) {
        return invalidJobError();
    }

    const std::string lastError = job->engine.lastError();
    if (lastError.empty()) {
        return ErrorInfo::success();
    }

    return ErrorInfo::ffmpegFailure(lastError);
}

LocalVideoTranscodeProgress getLocalVideoTranscodeLastProgress(const LocalVideoTranscodeJobHandle& job)
{
    if (!job) {
        return {};
    }

    std::lock_guard<std::mutex> lock(job->mutex);
    return job->report.lastProgress;
}

} // namespace media
