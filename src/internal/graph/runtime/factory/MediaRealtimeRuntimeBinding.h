#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaUnboundGraphRuntime final {};

struct MediaRealtimeVideoRuntimeBinding final {
    MediaRealtimeVideoRuntimePlan runtime;
    std::optional<MediaRealtimeRtpTransportPlan> inputTransport;
};

using MediaRealtimeRuntimeBinding = std::variant<
    MediaUnboundGraphRuntime,
    MediaRealtimeVideoRuntimeBinding,
    MediaAvSyncRuntimeBinding>;

} // namespace media::ffmpeg::graph
