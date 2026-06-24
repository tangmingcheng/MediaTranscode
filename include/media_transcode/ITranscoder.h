#pragma once

#include "MediaTranscodeTypes.h"
#include "Result.h"

#include <memory>
#include <string>

namespace media {

/**
 * @brief Low-level transcoder engine interface.
 *
 * Third-party applications should normally use TranscodeSession or the free
 * function transcode() from MediaTranscode.h. ITranscoder remains useful when
 * embedding a custom engine implementation or when an application needs direct
 * control over initialize / start / stop / wait.
 *
 * Compatibility note: the original bool-returning methods are kept so existing
 * code continues to build. New code can use configure(), startAsync(),
 * requestStop() and waitForFinished(), which convert the legacy bool contract
 * into Status.
 */
class ITranscoder {
public:
    virtual ~ITranscoder() = default;

    /**
     * @brief Configure the engine. Must be called before start().
     * @return true on success. On failure, lastError() should describe the reason.
     */
    virtual bool initialize(const TranscodeConfig& config) = 0;

    /**
     * @brief Start transcoding asynchronously.
     *
     * Implementations should return quickly and run the heavy FFmpeg loop in a
     * worker thread. Use wait() / waitForFinished() to wait for natural finish.
     */
    virtual bool start() = 0;

    /**
     * @brief Request stop and release worker resources.
     *
     * Implementations are expected to be idempotent. Calling stop() on an idle
     * engine should be safe.
     */
    virtual void stop() = 0;

    /**
     * @brief Optional external frame input.
     *
     * File / URL transcoders may return false because they pull frames from the
     * configured inputUrl. Realtime frame encoders can implement this later with
     * a typed frame wrapper. Avoid exposing raw void* in new high-level APIs.
     */
    virtual bool pushFrame(void* frame) = 0;

    /**
     * @brief Set progress callback. Passing an empty callback disables progress events.
     */
    virtual void setProgressCallback(ProgressCallback cb) = 0;

    /**
     * @brief Wait until the current asynchronous job finishes.
     *
     * Engines that do not support asynchronous execution can keep the default.
     */
    virtual bool wait()
    {
        return !isRunning();
    }

    /**
     * @brief Whether a job is currently running.
     */
    virtual bool isRunning() const
    {
        return false;
    }

    /**
     * @brief Last diagnostic message. Empty string means no known error.
     */
    virtual std::string lastError() const
    {
        return {};
    }

    /**
     * @brief Status-returning wrapper for initialize().
     */
    [[nodiscard]] Status configure(const TranscodeConfig& config)
    {
        if (initialize(config)) {
            return Status::success();
        }
        return Status::failure(ErrorInfo::invalidArgument(lastErrorOr("initialize failed")));
    }

    /**
     * @brief Status-returning wrapper for start().
     */
    [[nodiscard]] Status startAsync()
    {
        if (start()) {
            return Status::success();
        }
        return Status::failure(ErrorInfo::internalError(lastErrorOr("start failed")));
    }

    /**
     * @brief Status-returning wrapper for stop().
     */
    [[nodiscard]] Status requestStop()
    {
        stop();
        return Status::success();
    }

    /**
     * @brief Wait for finish and convert the last error into Status.
     */
    [[nodiscard]] Status waitForFinished()
    {
        if (!wait()) {
            return Status::failure(ErrorInfo::internalError(lastErrorOr("wait failed")));
        }

        const std::string error = lastError();
        if (!error.empty()) {
            return Status::failure(ErrorInfo::ffmpegFailure(error));
        }

        return Status::success();
    }

private:
    std::string lastErrorOr(const char* fallback) const
    {
        const std::string error = lastError();
        return error.empty() ? std::string(fallback) : error;
    }
};

using ITranscoderPtr = std::shared_ptr<ITranscoder>;

} // namespace media
