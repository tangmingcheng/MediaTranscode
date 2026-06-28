#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaTraceEventKind {
    RuntimeStart,
    RuntimeStop,
    NodeStart,
    NodeStop,
    BufferPush,
    BufferPop,
    Backpressure,
    Error,
    Custom
};

struct MediaTraceEvent {
    MediaTraceEventKind kind = MediaTraceEventKind::Custom;
    MediaNodeId nodeId = MediaNodeId::invalid();
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point timestamp;
    std::string name;
    std::string message;
};

} // namespace media::ffmpeg::graph
