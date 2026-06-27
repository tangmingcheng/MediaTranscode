#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaBufferAllocator {
public:
    virtual ~MediaBufferAllocator() = default;

    MediaBufferAllocator(const MediaBufferAllocator&) = delete;
    MediaBufferAllocator& operator=(const MediaBufferAllocator&) = delete;

    virtual ::media::Result<MediaBufferRef> allocate() = 0;
    virtual void release(MediaBufferRef buffer);

protected:
    MediaBufferAllocator() = default;
};

} // namespace media::ffmpeg::graph
