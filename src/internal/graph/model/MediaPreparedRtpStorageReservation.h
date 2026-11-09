#pragma once

#include "internal/graph/runtime/resource/MediaPreparedRtpSourceBudgets.h"
#include <optional>
#include <cstddef>
#include <memory>
#include <string>

namespace media::ffmpeg::graph {

// Identity is the actual shared allocation budget, not equality of byte counts.
struct MediaPreparedRtpStorageReservation final {
    std::shared_ptr<const void> allocationOwner;
    std::size_t maximumAllocationBytes;
    std::size_t streamCount;
    std::string authority;
    // Continuous sources retain all direct-child identities; raw bootstrap has none.
    std::optional<MediaPreparedRtpSourceBudgets> sourceBudgets;
};

} // namespace media::ffmpeg::graph
