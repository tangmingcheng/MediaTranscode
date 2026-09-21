#pragma once

#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

namespace media::ffmpeg::graph {

struct MediaCanonicalAudioContribution final {
    MediaCanonicalSourceStamp source;
    MediaCanonicalAudioSampleInterval interval;
};

} // namespace media::ffmpeg::graph
