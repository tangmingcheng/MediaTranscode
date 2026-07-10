#pragma once

#include "internal/graph/core/MediaNode.h"
#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeBinding.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRuntimeNodeFactory final {
public:
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>> create(const MediaNode& node);
    static ::media::Result<std::unique_ptr<MediaRuntimeNode>> create(
        const MediaNode& node,
        MediaPreparedRealtimeInputBinding* binding);
    static bool supported(MediaNodeKind kind) noexcept;
};

} // namespace media::ffmpeg::graph
