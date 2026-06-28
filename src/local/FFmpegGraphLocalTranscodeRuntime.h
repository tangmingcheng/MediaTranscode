#pragma once

#include "internal/TranscodeTypes.h"
#include "media_transcode/Result.h"

#include <atomic>

namespace media::ffmpeg {

class FFmpegGraphLocalTranscodeRuntime final {
public:
    struct Config {
        TranscodeConfig transcodeConfig;
        std::atomic_bool* stopRequested = nullptr;
        ProgressCallback progressCallback;
    };

    FFmpegGraphLocalTranscodeRuntime() = default;

    FFmpegGraphLocalTranscodeRuntime(const FFmpegGraphLocalTranscodeRuntime&) = delete;
    FFmpegGraphLocalTranscodeRuntime& operator=(const FFmpegGraphLocalTranscodeRuntime&) = delete;

    Status run(Config config);
};

} // namespace media::ffmpeg
