#pragma once

#include "internal/graph/runtime/profiling/MediaGraphProfiler.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphBottleneck {
    MediaNodeId nodeId = MediaNodeId::invalid();
    int64_t averageUs = 0;
    int64_t maxUs = 0;
    uint64_t calls = 0;
    std::string reason;
};

class MediaGraphBottleneckDetector final {
public:
    static std::vector<MediaGraphBottleneck> detect(const MediaGraphProfiler& profiler,
                                                    int64_t averageThresholdUs = 1000);
};

} // namespace media::ffmpeg::graph
