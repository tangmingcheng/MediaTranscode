#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphRuntime;

struct MediaRealtimeRuntimeCompletionOutcome final {
    ::media::Status status;
    bool stopAttempted = false;
};

class MediaRealtimeRuntimeCompletion final {
public:
    static MediaRealtimeRuntimeCompletionOutcome complete(
        MediaGraphRuntime& runtime,
        const ::media::Status& waitStatus);
};

} // namespace media::ffmpeg::graph
