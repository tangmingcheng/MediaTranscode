#pragma once

#include "internal/graph/model/MediaVideoSourceTiming.h"

namespace media::ffmpeg::graph {

enum class MediaRtpVideoPreparationIntent {
    CanonicalIntervals,
    SourcePresentation
};

} // namespace media::ffmpeg::graph
