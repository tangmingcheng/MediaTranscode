#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

struct MediaTsAccessUnitView final {
    std::span<const std::uint8_t> payload;
    MediaScheduledStream stream;
    std::uint64_t generation;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime dispatchOnMaster;
    MediaRunningTime emitOnMaster;
    bool randomAccess;
};

} // namespace media::ffmpeg::graph
