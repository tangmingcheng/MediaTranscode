#pragma once
#include "internal/graph/sync/MediaCanonicalAccessUnitIdentity.h"
#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {
struct MediaAvSchedulerVideoProgress final {
    std::uint64_t generation;
    MediaCanonicalAccessUnitIdentity timeline;
    MediaRunningTime followingAccessUnitsLowerBound;
};
}
