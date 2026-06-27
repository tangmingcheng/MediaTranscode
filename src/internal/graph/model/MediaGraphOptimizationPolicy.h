#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaZeroCopyPolicy.h"

namespace media::ffmpeg::graph {

enum class MediaGraphOptimizationLevel {
    None,
    Safe,
    Aggressive,
    Realtime
};

struct MediaGraphOptimizationPolicy {
    MediaGraphOptimizationLevel level = MediaGraphOptimizationLevel::Safe;
    MediaLatencyPolicy latencyPolicy;
    MediaThreadingPolicy threadingPolicy;
    MediaZeroCopyPolicy zeroCopyPolicy;

    bool enableNodeFusion = false;
    bool enableRedundantTransferElimination = true;
    bool enableQueuePolicyTuning = true;
    bool enableBackpressurePlanning = true;
    bool enableDiagnosticReport = true;

    constexpr bool enabled() const noexcept
    {
        return level != MediaGraphOptimizationLevel::None;
    }
};

} // namespace media::ffmpeg::graph
