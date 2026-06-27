#pragma once

#include <memory>
#include <utility>

namespace media::ffmpeg::graph {

class MediaBuffer;

using MediaBufferRef = std::shared_ptr<MediaBuffer>;
using MediaBufferWeakRef = std::weak_ptr<MediaBuffer>;

template <typename Buffer, typename... Args>
MediaBufferRef makeMediaBufferRef(Args&&... args)
{
    return std::make_shared<Buffer>(std::forward<Args>(args)...);
}

} // namespace media::ffmpeg::graph
