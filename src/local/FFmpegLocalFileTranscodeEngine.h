#pragma once

#include "media_transcode/FFmpegTranscoder.h"

#include <string>

namespace media {

/**
 * @brief Internal capability-specific engine for local video file transcoding.
 *
 * The old FFmpegTranscoder remains a private delegate. This class is the
 * implementation boundary used by the public local-video transcode API.
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
    FFmpegTranscoder m_delegate;
};

} // namespace media
