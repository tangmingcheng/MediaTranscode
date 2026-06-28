#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphRunLoop final {
public:
    static ::media::Result<MediaGraphRunResult> run(MediaGraphRuntime& runtime,
                                                    bool startIfNeeded = true,
                                                    bool stopOnCompletion = false);
};

} // namespace media::ffmpeg::graph
