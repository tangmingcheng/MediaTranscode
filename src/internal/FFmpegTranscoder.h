#pragma once

#include "internal/FFmpegTranscodeTypes.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace media {

class FFmpegTranscoder {
public:
    FFmpegTranscoder();
    ~FFmpegTranscoder();

    FFmpegTranscoder(const FFmpegTranscoder&) = delete;
    FFmpegTranscoder& operator=(const FFmpegTranscoder&) = delete;

    bool initialize(const ffmpeg::TranscodeConfig& config);
    bool start();
    void stop();
    bool wait();
    bool isRunning() const;

    void setProgressCallback(ProgressCallback cb);
    std::string lastError() const;

private:
    void transcodeThread();
    void setLastError(const std::string& error);
    void clearLastError();

private:
    ffmpeg::TranscodeConfig m_config;

    std::thread m_transcodeThread;
    std::atomic_bool m_running{ false };
    std::atomic_bool m_stopRequested{ false };

    mutable std::mutex m_mutex;
    std::string m_lastError;

    ProgressCallback m_progressCallback;
};

} // namespace media
