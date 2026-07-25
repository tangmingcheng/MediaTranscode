#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

::media::Result<bool> parseMediaVideoLineageCopyOpaqueOption(
    const MediaNodeOptions* options);

} // namespace media::ffmpeg::graph
