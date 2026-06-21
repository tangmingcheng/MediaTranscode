#pragma once

#include "media_transcode/MediaTranscodeTypes.h"
#include "media_transcode/Result.h"

namespace media {

class BitratePlanner {
public:
    struct Input {
        VideoCodec codec = VideoCodec::H264;
        VideoBitrateControlOptions options;
        VideoBitrateControlPolicy policy;

        int outputWidth = 0;
        int outputHeight = 0;
        int outputFps = 0;
    };

    static Result<VideoBitratePlan> plan(const Input& input);
};

} // namespace media
