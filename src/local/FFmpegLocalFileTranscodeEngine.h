#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace media {

/**
 * @brief Internal capability-specific engine for local video file transcoding.
 *
 * This class owns the FFmpeg worker thread and wires the local-file capability
 * to the reusable video/audio pipeline stages.
 */
class FFmpegLocalFileTranscodeEngine {
public:
    FFmpegLocalFileTranscodeEngine();
    ~FFmpegLocalFileTranscodeEngine();

    FFmpegLocalFileTranscodeEngine(const FFmpegLocalFileTranscodeEngine&) = delete;
    FFmpegLocalFileTranscodeEngine& operator=(const FFmpegLocalFileTranscodeEngine&) = delete;

    bool initialize(const TranscodeConfig& config);
    bool start();
    void stop();
    bool wait();
    bool isRunning() const;
    std::string lastError() const;
    void setProgressCallback(ProgressCallback cb);

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
