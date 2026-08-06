#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaTranscodeStreamSetRequestValidator final {
public:
    static ::media::Status validate(const MediaTranscodeParameterSet& parameters);

private:
    MediaTranscodeStreamSetRequestValidator() = delete;
};

} // namespace media::ffmpeg::graph
