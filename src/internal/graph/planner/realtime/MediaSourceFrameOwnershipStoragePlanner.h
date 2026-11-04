#pragma once
#include "internal/graph/planner/video/MediaVideoFrameOwnershipStoragePlanner.h"
#include <optional>
#include <cstdint>

namespace media::ffmpeg::graph {
struct MediaSourceFrameOwnershipStoragePlan final {
    std::uint64_t decoderOperations;
    std::optional<std::uint64_t> filterOperations;
    std::uint64_t inputSnapshots;
    std::uint64_t bytes;
};
// Conservative reference bounds may count aliases more than once. This is a
// preparation ownership component, not payload/device storage or full admission.
class MediaSourceFrameOwnershipStoragePlanner final {
public:
    static ::media::Result<MediaSourceFrameOwnershipStoragePlan> plan(
        const MediaVideoFrameOwnershipStoragePlan& decoder,
        const std::optional<MediaVideoFrameOwnershipStoragePlan>& filter,
        std::uint64_t decoderReferenceBound,
        std::optional<std::uint64_t> filterReferenceBound);
};
} // namespace media::ffmpeg::graph
