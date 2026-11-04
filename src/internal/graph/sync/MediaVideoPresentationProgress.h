#pragma once

#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {

// Producer assertion: every frame published after the carrying frame, in its
// lineage's timeline and generation, has presentation >= this bound.
// This is not a frame duration or an encoded decode-time guarantee.
struct MediaVideoPresentationProgress final {
    MediaRunningTime followingFramesLowerBound;
};

} // namespace media::ffmpeg::graph
