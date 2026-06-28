#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaStreamingMetrics {
    uint64_t packetsSent = 0;
    uint64_t packetsDropped = 0;
    uint64_t bytesSent = 0;
    int64_t estimatedThroughputBitsPerSecond = 0;
    int64_t queueDelayUs = 0;
    double packetLossRate = 0.0;
};

} // namespace media::ffmpeg::graph
