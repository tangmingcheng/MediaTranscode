#pragma once

#include "internal/graph/runtime/resource/MediaBudgetMemoryResource.h"
#include "internal/graph/runtime/resource/MediaStorageBudget.h"

namespace media::ffmpeg::graph {

// Runtime userspace network allocations; independent of preparation and socket budgets.
struct MediaNetworkStorageDomain final {};
using MediaNetworkStorageBudget = MediaStorageBudget<MediaNetworkStorageDomain>;
using MediaNetworkStorageLease = MediaStorageLease<MediaNetworkStorageDomain>;
using MediaNetworkMemoryResource = MediaBudgetMemoryResource<
    MediaNetworkStorageBudget, MediaNetworkStorageLease>;

} // namespace media::ffmpeg::graph
