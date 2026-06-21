#pragma once

#include "media_transcode/MediaTranscodeTypes.h"

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg {

    struct VideoEncodeOptionsApplyReport {
        int64_t bitRate = 0;
        int64_t maxBitRate = 0;
        int64_t bufferSize = 0;
        int gopSize = 0;
        int maxBFrames = 0;

        bool rateControlApplied = false;
        bool presetApplied = false;
        bool tuneApplied = false;
        bool profileApplied = false;
        bool levelApplied = false;

        std::vector<std::string> appliedOptions;
        std::vector<std::string> unsupportedOptions;
        std::vector<std::string> failedOptions;

        std::string describe() const;
    };

    class VideoEncodeOptionsApplier {
    public:
        static VideoEncodeOptionsApplyReport apply(AVCodecContext* encoderContext,
                                                   const AVCodec* encoder,
                                                   const TranscodeConfig& config,
                                                   int outputFps);
    };

} // namespace media::ffmpeg
