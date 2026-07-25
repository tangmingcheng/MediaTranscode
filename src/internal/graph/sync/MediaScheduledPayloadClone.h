#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaScheduledPayloadClone final {
public:
    static ::media::Result<MediaBufferRef> clonePacket(const MediaBufferRef& source);
private:
    MediaScheduledPayloadClone() = delete;
};

} // namespace media::ffmpeg::graph
