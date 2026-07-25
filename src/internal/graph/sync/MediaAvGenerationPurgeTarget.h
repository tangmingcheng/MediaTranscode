#pragma once

#include "internal/graph/sync/MediaAvGenerationTransition.h"

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget {
public:
    virtual ~MediaAvGenerationPurgeTarget() = default;
    virtual ::media::Status purge(const MediaAvGenerationPurge& purge) = 0;
};

} // namespace media::ffmpeg::graph
