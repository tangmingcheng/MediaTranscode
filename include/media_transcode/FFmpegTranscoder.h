#pragma once

#include "media_transcode/ITranscoder.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace media {

/**
 * @brief FFmpeg-backed single-input transcoder engine.
 *
 * This class is retained as the current implementation behind the public
 * transcodeLocalFile() API. Third-party applications should include only
 * media_transcode/MediaTranscode.h and should not depend on this class directly.
 *
 * The implementation is intentionally left in its existing shape for now so the
 * public API refactor does not rewrite the working FFmpeg pipeline.
 */
class FFmpegTranscoder : public ITranscoder {
public:
    FFmpegTranscoder();
    ~FFmpegTranscoder() override;

    FFmpegTranscoder(const FFmpegTranscoder&) = delete;
    FFmpegTranscoder& operator=(const FFmpegTranscoder&) = delete;

    bool initialize(const TranscodeConfig& config) override;
    bool start() override;
    void stop() override;
    bool pushFrame(void* frame) override;
    void setProgressCallback(ProgressCallback cb) override;

    bool wait() override;
    bool isRunning() const override;
    std::string lastError() const override;

private:
    void transcodeThread();
    void setLastError(const std::string& error);
    void clearLastError();

private:
    TranscodeConfig m_config;

    std::thread m_transcodeThread;
    std::atomic_bool m_running{ false };
    std::atomic_bool m_stopRequested{ false };

    mutable std::mutex m_mutex;
    std::string m_lastError;

    ProgressCallback m_progressCallback;
};

} // namespace media
