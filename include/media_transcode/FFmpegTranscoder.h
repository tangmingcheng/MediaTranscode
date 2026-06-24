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
 * FFmpegTranscoder is the low-level engine used by TranscodeSession. It supports
 * file / URL input and file / URL output through TranscodeConfig::inputUrl and
 * TranscodeConfig::outputUrl.
 *
 * Typical third-party usage should prefer:
 *
 * @code
 * auto result = media::transcode(
 *     media::TranscodeConfig::make("input.mp4", "output.mp4")
 *         .setVideoSize(1280, 720)
 *         .setVideoBitrate(3000));
 * @endcode
 *
 * Use this class directly only when you need explicit lifecycle control.
 */
class FFmpegTranscoder : public ITranscoder {
public:
    FFmpegTranscoder();
    ~FFmpegTranscoder() override;

    FFmpegTranscoder(const FFmpegTranscoder&) = delete;
    FFmpegTranscoder& operator=(const FFmpegTranscoder&) = delete;

    /**
     * @brief Configure the transcoder. This method is kept for compatibility.
     *
     * New code can call configure(), inherited from ITranscoder, to get Status.
     */
    bool initialize(const TranscodeConfig& config) override;

    /**
     * @brief Start transcoding in a worker thread. This method is kept for compatibility.
     *
     * New code can call startAsync(), inherited from ITranscoder, to get Status.
     */
    bool start() override;

    /**
     * @brief Request stop and wait for the worker thread to exit.
     */
    void stop() override;

    /**
     * @brief File / URL transcoder does not accept externally pushed frames.
     *
     * Realtime frame input should be introduced through a dedicated typed API in
     * a later realtime encoder component, not through this file transcoder.
     */
    bool pushFrame(void* frame) override;

    void setProgressCallback(ProgressCallback cb) override;

    /**
     * @brief Wait until a previously started asynchronous transcode job finishes.
     */
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
