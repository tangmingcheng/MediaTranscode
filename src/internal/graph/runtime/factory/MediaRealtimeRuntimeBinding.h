#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaUnboundGraphRuntime final {};

struct MediaRealtimeVideoRuntimeBinding final {
    MediaRealtimeVideoRuntimePlan runtime;
};

using MediaRealtimeRuntimeBinding = std::variant<
    MediaUnboundGraphRuntime,
    MediaRealtimeVideoRuntimeBinding,
    MediaAvSyncRuntimeBinding>;

} // namespace media::ffmpeg::graph
