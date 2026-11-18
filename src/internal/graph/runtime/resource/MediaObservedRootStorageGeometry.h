#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
namespace media::ffmpeg::graph {
struct MediaObservedRootStorageGeometry final {
    std::size_t permanentBytes;
    std::size_t temporaryBytes;
    std::size_t totalBytes;
    static ::media::Result<MediaObservedRootStorageGeometry> plan(std::size_t targets);
};
}
