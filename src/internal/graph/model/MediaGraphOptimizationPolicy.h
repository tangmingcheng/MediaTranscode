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
    MediaGraphOptimizationLevel level = MediaGraphOptimizationLevel::None;
    MediaLatencyPolicy latencyPolicy;
    MediaThreadingPolicy threadingPolicy;
    MediaZeroCopyPolicy zeroCopyPolicy;

    bool enableNodeFusion = false;
    bool enableRedundantTransferElimination = false;
    bool enableQueuePolicyTuning = false;
    bool enableBackpressurePlanning = false;
    bool enableDiagnosticReport = true;

    constexpr bool enabled() const noexcept
    {
        return level != MediaGraphOptimizationLevel::None ||
               enableNodeFusion ||
               enableRedundantTransferElimination ||
               enableQueuePolicyTuning ||
               enableBackpressurePlanning;
    }
};

} // namespace media::ffmpeg::graph
