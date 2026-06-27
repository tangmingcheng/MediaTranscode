#pragma once

#include "internal/graph/runtime/MediaGraphExecutionContext.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaBackpressureDecisionKind {
    None,
    Healthy,
    AboveHighWatermark,
    AboveCriticalWatermark,
    QueueFull
};

struct MediaBackpressureDecision {
    MediaBackpressureDecisionKind kind = MediaBackpressureDecisionKind::None;
    MediaEdgeId edgeId = MediaEdgeId::invalid();
    std::size_t queueSize = 0;
    std::size_t capacity = 0;
    std::string message;
};

struct MediaBackpressureReport {
    std::vector<MediaBackpressureDecision> decisions;

    bool healthy() const noexcept
    {
        for (const auto& item : decisions) {
            if (item.kind == MediaBackpressureDecisionKind::AboveCriticalWatermark ||
                item.kind == MediaBackpressureDecisionKind::QueueFull) {
                return false;
            }
        }
        return true;
    }
};

class MediaBackpressureController final {
public:
    static MediaBackpressureReport inspect(const MediaGraphExecutionContext& context);
};

} // namespace media::ffmpeg::graph
