#include "internal/graph/runtime/diagnostics/MediaRuntimeMetricsCollector.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

namespace media::ffmpeg::graph {
MediaGraphRuntimeMetrics MediaRuntimeMetricsCollector::workers(
    std::span<const std::unique_ptr<MediaGraphWorker>> workers)
{
    MediaGraphRuntimeMetrics metrics;
    metrics.threadCount = workers.size();
    for (const auto& worker : workers) {
        if (!worker) continue;
        if (worker->running()) ++metrics.activeWorkers;
        metrics.workerProgress += worker->metrics().progress;
        metrics.workerProcessCalls += worker->metrics().processCalls;
        metrics.workerWaits += worker->metrics().waits;
        metrics.workerWakeups += worker->metrics().wakeups;
        metrics.workerErrors += worker->metrics().errors;
    }
    metrics.workerIterations = metrics.workerProcessCalls;
    metrics.errorCount = metrics.workerErrors;
    return metrics;
}
void MediaRuntimeMetricsCollector::includeChannel(
    MediaGraphRuntimeMetrics& metrics, const MediaChannel& channel)
{
    metrics.totalPushed += channel.metrics().pushed;
    metrics.totalPopped += channel.metrics().popped;
    metrics.droppedBuffers += channel.metrics().queue.dropped;
    metrics.queuedBuffers += channel.size();
    metrics.peakQueuedBuffers += channel.metrics().queue.peakSize;
    if (channel.binding().edgeKind == MediaEdgeKind::EncodedPacket) {
        metrics.encodedPacketsPushed += channel.metrics().pushed;
        metrics.encodedPacketsPopped += channel.metrics().popped;
    }
}
} // namespace media::ffmpeg::graph
