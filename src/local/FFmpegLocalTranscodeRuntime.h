#pragma once

#include "internal/TranscodeTypes.h"
#include "media_transcode/Result.h"

#include <atomic>

namespace media::ffmpeg {

class FFmpegLocalTranscodeRuntime final {
public:
    struct Config {
        TranscodeConfig transcodeConfig;
        std::atomic_bool* stopRequested = nullptr;
        ProgressCallback progressCallback;
    };

    FFmpegLocalTranscodeRuntime() = default;

    FFmpegLocalTranscodeRuntime(const FFmpegLocalTranscodeRuntime&) = delete;
    FFmpegLocalTranscodeRuntime& operator=(const FFmpegLocalTranscodeRuntime&) = delete;

    Status run(Config config);
};

} // namespace media::ffmpeg
