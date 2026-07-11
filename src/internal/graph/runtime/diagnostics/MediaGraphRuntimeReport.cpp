#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

std::string MediaGraphRuntimeReport::summary() const
{
    return "runtime report: queued=" + std::to_string(metrics.queuedBuffers) +
           ", peakQueued=" + std::to_string(metrics.peakQueuedBuffers) +
           ", threads=" + std::to_string(metrics.threadCount) +
           ", processThreads=" + std::to_string(metrics.processThreadCount) +
           ", workers=" + std::to_string(metrics.activeWorkers) +
           ", workerProgress=" + std::to_string(metrics.workerProgress) +
           ", workerProcessCalls=" + std::to_string(metrics.workerProcessCalls) +
           ", workerWaits=" + std::to_string(metrics.workerWaits) +
           ", workerWakeups=" + std::to_string(metrics.workerWakeups) +
           ", workerErrors=" + std::to_string(metrics.workerErrors) +
           ", errors=" + std::to_string(metrics.errorCount) +
           ", stalledIntervals=" + std::to_string(metrics.stalledIntervals) +
           ", cpuSamples=" + std::to_string(metrics.cpuSampleCount) +
           ", averageCpuPercent=" + std::to_string(metrics.averageCpuPercent) +
           ", averageProcessCpuPercent=" + std::to_string(metrics.averageProcessCpuPercent) +
           ", workingSetBytes=" + std::to_string(metrics.workingSetBytes) +
           ", totalPushed=" + std::to_string(metrics.totalPushed) +
           ", totalPopped=" + std::to_string(metrics.totalPopped) +
           ", droppedBuffers=" + std::to_string(metrics.droppedBuffers) +
           ", encodedPacketsPushed=" + std::to_string(metrics.encodedPacketsPushed) +
           ", encodedPacketsPopped=" + std::to_string(metrics.encodedPacketsPopped) +
           ", backpressureItems=" + std::to_string(backpressure.decisions.size());
}

MediaGraphRuntimeReport MediaGraphRuntimeReporter::capture(MediaGraphRuntime& runtime)
{
    (void)runtime.synchronizeThreadedState();
    return capture(static_cast<const MediaGraphRuntime&>(runtime));
}

MediaGraphRuntimeReport MediaGraphRuntimeReporter::capture(const MediaGraphRuntime& runtime)
{
    MediaGraphRuntimeReport report;
    report.state = runtime.state();
    const MediaGraphRuntimeMetrics acceptance = runtime.acceptanceCollector().snapshot();
    report.metrics.cpuSampleCount = acceptance.cpuSampleCount;
    report.metrics.averageCpuPercent = acceptance.averageCpuPercent;
    report.metrics.stalledIntervals = acceptance.stalledIntervals;
    report.metrics.errorCount = acceptance.errorCount;
    report.metrics.averageProcessCpuPercent = acceptance.averageProcessCpuPercent;
    report.metrics.workingSetBytes = acceptance.workingSetBytes;
    report.metrics.processThreadCount = acceptance.processThreadCount;

    std::size_t queued = 0;
    for (const MediaChannel* channel : runtime.context().channels().channels()) {
        if (channel) {
            queued += channel->size();
            report.metrics.totalPushed += channel->metrics().pushed;
            report.metrics.totalPopped += channel->metrics().popped;
            report.metrics.droppedBuffers += channel->metrics().queue.dropped;
            if (channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
                report.metrics.encodedPacketsPushed += channel->metrics().pushed;
                report.metrics.encodedPacketsPopped += channel->metrics().popped;
            }
        }
    }

    const std::size_t graphQueuePeak = runtime.observeQueueHighWatermark(queued);
    report.metrics.updateQueuedBuffers(queued, graphQueuePeak);
    const MediaGraphRuntimeMetrics executorMetrics = runtime.threadedExecutor().metrics();
    if (executorMetrics.threadCount != 0 || executorMetrics.workerErrors != 0) {
        const uint64_t totalPushed = report.metrics.totalPushed;
        const uint64_t totalPopped = report.metrics.totalPopped;
        const uint64_t droppedBuffers = report.metrics.droppedBuffers;
        const uint64_t encodedPacketsPushed = report.metrics.encodedPacketsPushed;
        const uint64_t encodedPacketsPopped = report.metrics.encodedPacketsPopped;
        report.metrics = executorMetrics;
        report.metrics.cpuSampleCount = acceptance.cpuSampleCount;
        report.metrics.averageCpuPercent = acceptance.averageCpuPercent;
        report.metrics.stalledIntervals = acceptance.stalledIntervals;
        report.metrics.errorCount += acceptance.errorCount;
        report.metrics.totalPushed = totalPushed;
        report.metrics.totalPopped = totalPopped;
        report.metrics.droppedBuffers = droppedBuffers;
        report.metrics.encodedPacketsPushed = encodedPacketsPushed;
        report.metrics.encodedPacketsPopped = encodedPacketsPopped;
        report.metrics.averageProcessCpuPercent = acceptance.averageProcessCpuPercent;
        report.metrics.workingSetBytes = acceptance.workingSetBytes;
        report.metrics.processThreadCount = acceptance.processThreadCount;
        report.metrics.updateQueuedBuffers(queued, graphQueuePeak);
    }

    report.backpressure = MediaBackpressureController::inspect(runtime.context());
    return report;
}

} // namespace media::ffmpeg::graph
