#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaChannel;

::media::Result<std::optional<MediaBufferRef>> tryReadRequiredInput(
    MediaChannel* channel,
    std::string_view consumer,
    std::string_view inputName);

} // namespace media::ffmpeg::graph
