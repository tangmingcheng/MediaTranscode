#pragma once

#include "internal/TranscodeTypes.h"

namespace media::ffmpeg {

    class VideoBitrateControlPlanner {
    public:
        static VideoBitratePlan plan(const TranscodeConfig& config,
                                     int outputWidth,
                                     int outputHeight,
                                     int outputFps);
    };

} // namespace media::ffmpeg
