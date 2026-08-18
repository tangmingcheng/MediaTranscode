#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

::media::Result<std::string> requiredNodeOption(const MediaNodeOptions* options,
                                                const char* nodeName,
                                                const char* key);

::media::Result<std::string> requiredPossiblyEmptyNodeOption(const MediaNodeOptions* options,
                                                             const char* nodeName,
                                                             const char* key);

::media::Result<int> requiredPositiveIntNodeOption(const MediaNodeOptions* options,
                                                   const char* nodeName,
                                                   const char* key);

::media::Result<int> requiredNonNegativeIntNodeOption(const MediaNodeOptions* options,
                                                      const char* nodeName,
                                                      const char* key);

::media::Result<std::int64_t> requiredPositiveInt64NodeOption(const MediaNodeOptions* options,
                                                              const char* nodeName,
                                                              const char* key);

::media::Result<bool> requiredBoolNodeOption(const MediaNodeOptions* options,
                                             const char* nodeName,
                                             const char* key);

::media::Result<MediaStreamKind> requiredStreamKindNodeOption(const MediaNodeOptions* options,
                                                              const char* nodeName,
                                                              const char* key);

} // namespace media::ffmpeg::graph
