#pragma once

#include <cstdint>
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaGraphRuntimeMetrics {
    uint64_t processIterations = 0;
    uint64_t workerIterations = 0;
    uint64_t workerProcessCalls = 0;
    uint64_t workerProgress = 0;
    uint64_t workerWaits = 0;
    uint64_t workerWakeups = 0;
    uint64_t workerErrors = 0;
    uint64_t schedulerStarts = 0;
    uint64_t schedulerStops = 0;
    uint64_t schedulerAborts = 0;
    uint64_t totalPushed = 0;
    uint64_t totalPopped = 0;
    uint64_t droppedBuffers = 0;
    uint64_t encodedPacketsPushed = 0;
    uint64_t encodedPacketsPopped = 0;
    uint64_t cpuSampleCount = 0;
    uint64_t stalledIntervals = 0;
    uint64_t errorCount = 0;

    std::size_t threadCount = 0;
    std::size_t processThreadCount = 0;
    std::size_t activeWorkers = 0;
    std::size_t queuedBuffers = 0;
    std::size_t peakQueuedBuffers = 0;
    double averageCpuPercent = 0.0;
    double averageProcessCpuPercent = 0.0;
    std::uint64_t workingSetBytes = 0;

    void updateThreadCount(std::size_t total, std::size_t active) noexcept
    {
        threadCount = total;
        activeWorkers = active;
    }

    void updateQueuedBuffers(std::size_t current, std::size_t observedPeak = 0) noexcept
    {
        queuedBuffers = current;
        const std::size_t peak = observedPeak > current ? observedPeak : current;
        if (peak > peakQueuedBuffers) {
            peakQueuedBuffers = peak;
        }
    }

};

} // namespace media::ffmpeg::graph
