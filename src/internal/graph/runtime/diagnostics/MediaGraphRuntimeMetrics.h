#pragma once

#include <cstdint>
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaGraphRuntimeMetrics {
    uint64_t processIterations = 0;
    uint64_t workerIterations = 0;
    uint64_t workerErrors = 0;
    uint64_t schedulerStarts = 0;
    uint64_t schedulerStops = 0;
    uint64_t schedulerAborts = 0;

    std::size_t activeWorkers = 0;
    std::size_t queuedBuffers = 0;
    std::size_t peakQueuedBuffers = 0;

    void updateQueuedBuffers(std::size_t current) noexcept
    {
        queuedBuffers = current;
        if (queuedBuffers > peakQueuedBuffers) {
            peakQueuedBuffers = queuedBuffers;
        }
    }
};

} // namespace media::ffmpeg::graph
