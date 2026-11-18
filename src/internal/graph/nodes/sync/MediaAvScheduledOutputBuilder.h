#pragma once
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaAvScheduledOutputBuilder final {
public:
    static ::media::Result<MediaBufferRef> canonical(
        const MediaCanonicalAccessUnitBuffer& unit,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaRunningTime emitOnMaster);
};
} // namespace media::ffmpeg::graph
