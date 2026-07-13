#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"

namespace media::ffmpeg::graph {

struct MediaPreparedRealtimeInputBinding final {
    MediaNodeId nodeId;
    MediaPreparedRealtimeInputKind expectedKind;
    MediaPreparedRealtimeInput prepared;
};

} // namespace media::ffmpeg::graph
