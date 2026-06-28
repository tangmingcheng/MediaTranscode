#include "internal/graph/runtime/profiling/MediaGraphBottleneckDetector.h"

namespace media::ffmpeg::graph {

std::vector<MediaGraphBottleneck> MediaGraphBottleneckDetector::detect(const MediaGraphProfiler& profiler,
                                                                        int64_t averageThresholdUs)
{
    std::vector<MediaGraphBottleneck> result;

    for (const auto& item : profiler.nodeStats()) {
        const auto& stats = item.second;
        if (stats.averageUs() >= averageThresholdUs) {
            result.push_back({ MediaNodeId::fromValue(item.first),
                               stats.averageUs(),
                               stats.maxUs,
                               stats.calls,
                               "average processing time exceeded threshold" });
        }
    }

    return result;
}

} // namespace media::ffmpeg::graph
