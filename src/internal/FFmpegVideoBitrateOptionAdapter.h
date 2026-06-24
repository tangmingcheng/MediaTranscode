#pragma once

#include "internal/TranscodeTypes.h"

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg {

    struct VideoBitrateOption {
        enum class Type {
            String,
            Integer
        };

        Type type = Type::String;
        std::string name;
        std::string stringValue;
        int64_t integerValue = 0;
    };

    struct VideoBitrateOptionPlan {
        int64_t bitRate = 0;
        int64_t minBitRate = 0;
        int64_t maxBitRate = 0;
        int64_t bufferSize = 0;
        std::vector<VideoBitrateOption> privateOptions;
        std::vector<std::string> diagnostics;
    };

    class VideoBitrateOptionAdapter {
    public:
        static VideoBitrateOptionPlan adapt(const AVCodec* encoder,
                                            const VideoBitratePlan& plan);
    };

} // namespace media::ffmpeg
