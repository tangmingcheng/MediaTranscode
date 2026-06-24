#pragma once

#include "media_transcode/FFmpegTranscoder.h"
#include "media_transcode/Result.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace media {

/**
 * @brief High-level, library-friendly transcode session.
 *
 * TranscodeSession wraps an ITranscoder engine and exposes a Status / Result
 * based API. It is the recommended API for third-party C++ applications.
 *
 * Basic asynchronous usage:
 *
 * @code
 * media::TranscodeSession session;
 * auto start = session.startAsync(
 *     media::TranscodeConfig::make("input.mp4", "output.mp4")
 *         .setVideoSize(1280, 720)
 *         .setVideoBitrate(3000));
 * if (!start) {
 *     std::cerr << start.error().describe() << std::endl;
 * }
 * auto finished = session.wait();
 * @endcode
 */
class TranscodeSession {
public:
    TranscodeSession()
        : m_transcoder(std::make_shared<FFmpegTranscoder>())
    {
    }

    explicit TranscodeSession(ITranscoderPtr transcoder)
        : m_transcoder(std::move(transcoder))
    {
    }

    ~TranscodeSession()
    {
        if (m_transcoder && m_transcoder->isRunning()) {
            m_transcoder->stop();
        }
    }

    TranscodeSession(const TranscodeSession&) = delete;
    TranscodeSession& operator=(const TranscodeSession&) = delete;

    TranscodeSession(TranscodeSession&&) = delete;
    TranscodeSession& operator=(TranscodeSession&&) = delete;

    /**
     * @brief Configure the underlying engine without starting it.
     */
    [[nodiscard]] Status configure(TranscodeConfig config)
    {
        if (!m_transcoder) {
            return Status::failure(ErrorInfo::invalidArgument("transcoder engine is null"));
        }

        if (config.inputUrl.empty()) {
            return Status::failure(ErrorInfo::invalidArgument("inputUrl is empty"));
        }

        if (config.outputUrl.empty()) {
            return Status::failure(ErrorInfo::invalidArgument("outputUrl is empty"));
        }

        m_config = std::move(config);
        const Status status = m_transcoder->configure(m_config);
        m_configured = status.ok();
        return status;
    }

    /**
     * @brief Start an already configured session asynchronously.
     */
    [[nodiscard]] Status startAsync(ProgressCallback callback = {})
    {
        if (!m_transcoder) {
            return Status::failure(ErrorInfo::invalidArgument("transcoder engine is null"));
        }

        if (!m_configured) {
            return Status::failure(ErrorInfo::notInitialized("transcode session is not configured"));
        }

        m_transcoder->setProgressCallback([this, callback = std::move(callback)](const ProgressInfo& info) mutable {
            {
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_lastProgress = info;
            }

            if (callback) {
                callback(info);
            }
        });

        const Status status = m_transcoder->startAsync();
        m_started = status.ok();
        return status;
    }

    /**
     * @brief Configure and start the session asynchronously in one call.
     */
    [[nodiscard]] Status startAsync(TranscodeConfig config, ProgressCallback callback = {})
    {
        const Status configured = configure(std::move(config));
        if (!configured) {
            return configured;
        }
        return startAsync(std::move(callback));
    }

    /**
     * @brief Wait for natural completion and return a report.
     */
    [[nodiscard]] Result<TranscodeReport> wait()
    {
        if (!m_transcoder) {
            return Result<TranscodeReport>::failure(ErrorInfo::invalidArgument("transcoder engine is null"));
        }

        if (!m_started && !m_transcoder->isRunning()) {
            return Result<TranscodeReport>::failure(ErrorInfo::notInitialized("transcode session was not started"));
        }

        const Status status = m_transcoder->waitForFinished();
        m_started = false;

        if (!status) {
            return Result<TranscodeReport>::failure(status.error());
        }

        TranscodeReport report = makeReport();
        report.completed = true;
        report.stopped = false;
        return Result<TranscodeReport>::success(std::move(report));
    }

    /**
     * @brief Request stop. This call is idempotent.
     */
    [[nodiscard]] Status stop()
    {
        if (!m_transcoder) {
            return Status::failure(ErrorInfo::invalidArgument("transcoder engine is null"));
        }

        m_transcoder->stop();
        m_started = false;
        return Status::success();
    }

    bool isRunning() const
    {
        return m_transcoder && m_transcoder->isRunning();
    }

    const TranscodeConfig& config() const noexcept
    {
        return m_config;
    }

    ProgressInfo lastProgress() const
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        return m_lastProgress;
    }

    std::string lastError() const
    {
        return m_transcoder ? m_transcoder->lastError() : std::string("transcoder engine is null");
    }

    ErrorInfo lastErrorInfo(ErrorCode fallbackCode = ErrorCode::FFmpegFailure) const
    {
        const std::string error = lastError();
        if (error.empty()) {
            return ErrorInfo::success();
        }
        return ErrorInfo::make(fallbackCode, error);
    }

private:
    TranscodeReport makeReport() const
    {
        TranscodeReport report;
        report.config = m_config;
        report.lastProgress = lastProgress();
        return report;
    }

private:
    ITranscoderPtr m_transcoder;
    TranscodeConfig m_config;
    bool m_configured = false;
    bool m_started = false;

    mutable std::mutex m_progressMutex;
    ProgressInfo m_lastProgress;
};

/**
 * @brief Run one synchronous transcode job.
 *
 * This is the simplest library entry point. It configures, starts and waits for
 * completion before returning. Use TranscodeSession when you need cancellation
 * or separate start / wait control.
 */
inline Result<TranscodeReport> transcode(TranscodeConfig config,
                                         ProgressCallback callback = {})
{
    TranscodeSession session;
    const Status started = session.startAsync(std::move(config), std::move(callback));
    if (!started) {
        return Result<TranscodeReport>::failure(started.error());
    }
    return session.wait();
}

/**
 * @brief Convenience overload for common file / URL transcoding.
 */
inline Result<TranscodeReport> transcodeFile(std::string inputUrl,
                                             std::string outputUrl,
                                             ProgressCallback callback = {})
{
    return transcode(
        TranscodeConfig::make(std::move(inputUrl), std::move(outputUrl)),
        std::move(callback)
    );
}

} // namespace media
