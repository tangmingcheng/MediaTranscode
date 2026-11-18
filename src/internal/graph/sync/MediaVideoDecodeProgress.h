#pragma once
#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {
// Same canonical timeline and generation as the carrying complete video AU.
// Every following AU has canonical dispatch >= this bound.
struct MediaVideoDecodeProgress final {
    MediaRunningTime followingAccessUnitsLowerBound;
};
}
