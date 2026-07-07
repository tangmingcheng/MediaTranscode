#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

::media::Result<std::string> requiredNodeOption(const MediaNodeOptions* options,
                                                const char* nodeName,
                                                const char* key);

::media::Result<int> requiredPositiveIntNodeOption(const MediaNodeOptions* options,
                                                   const char* nodeName,
                                                   const char* key);

::media::Result<bool> requiredBoolNodeOption(const MediaNodeOptions* options,
                                             const char* nodeName,
                                             const char* key);

} // namespace media::ffmpeg::graph
