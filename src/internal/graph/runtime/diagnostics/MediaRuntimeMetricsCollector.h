#pragma once

#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include <memory>
#include <span>

namespace media::ffmpeg::graph {
class MediaGraphWorker;
class MediaChannel;
class MediaRuntimeMetricsCollector final {
public:
    static MediaGraphRuntimeMetrics workers(
        std::span<const std::unique_ptr<MediaGraphWorker>> workers);
    static void includeChannel(MediaGraphRuntimeMetrics& metrics,
                               const MediaChannel& channel);
};
} // namespace media::ffmpeg::graph
