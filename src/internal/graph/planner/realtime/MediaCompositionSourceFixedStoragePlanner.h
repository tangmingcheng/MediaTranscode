#pragma once
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"

namespace media::ffmpeg::graph {
struct MediaCompositionSourceFixedRequest;
class MediaCompositionSourceFixedStoragePlanner final {
public:
    // Covers new ordinary backing and construction peaks, not borrowed video
    // metadata or later independent graph copies. Object/control storage is
    // allocated through resource separately.
    static ::media::Result<std::shared_ptr<const MediaPreparationStorageLease>> reserve(
        const MediaCompositionSourceFixedRequest& request,
        const std::shared_ptr<MediaPreparationMemoryResource>& resource);
};
}
