#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/zerocopy/MediaZeroCopyPlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaHardwareFrameInterop final {
public:
    static ::media::Result<MediaBufferRef> apply(const MediaBufferRef& input,
                                                 const MediaZeroCopyPlan& plan);
};

} // namespace media::ffmpeg::graph
