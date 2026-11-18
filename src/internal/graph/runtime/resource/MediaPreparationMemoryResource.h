#pragma once

#include "internal/graph/runtime/resource/MediaBudgetMemoryResource.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"

namespace media::ffmpeg::graph {

// Preparation-only budget domain; runtime network storage has its own budget.
using MediaPreparationMemoryResource = MediaBudgetMemoryResource<
    MediaPreparationStorageBudget, MediaPreparationStorageLease>;

} // namespace media::ffmpeg::graph
