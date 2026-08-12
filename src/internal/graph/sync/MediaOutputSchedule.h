#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaOutputSchedule final {
    MediaRunningTime presentation;
    MediaRunningTime dispatch;
    MediaRunningTime emit;

    static ::media::Result<MediaOutputSchedule> create(
        MediaRunningTime presentation,
        MediaRunningTime dispatch,
        MediaRunningTime transportLead);
};

} // namespace media::ffmpeg::graph
