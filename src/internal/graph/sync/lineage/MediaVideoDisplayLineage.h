#pragma once

#include "internal/graph/sync/lineage/MediaVideoDecodeLineage.h"
#include <memory>

namespace media::ffmpeg::graph {

// A decoded display point, not an interval. Frame transformations preserve this
// identity even when the carrier's timestamp time base changes.
struct MediaVideoDisplayLineage final {
    std::shared_ptr<const MediaVideoDecodeLineage> accessUnit;
    std::uint64_t decodedSequence;
    MediaTimeValue decodedPresentation;
    MediaRational decodedTimeBase;
    MediaRunningTime presentation;
};

} // namespace media::ffmpeg::graph
