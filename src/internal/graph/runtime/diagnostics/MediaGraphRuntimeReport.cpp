#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

std::string MediaGraphRuntimeReport::summary() const
{
    return "runtime report: queued=" + std::to_string(metrics.queuedBuffers) +
           ", peakQueued=" + std::to_string(metrics.peakQueuedBuffers) +
           ", workers=" + std::to_string(metrics.activeWorkers) +
           ", workerProgress=" + std::to_string(metrics.workerProgress) +
           ", workerProcessCalls=" + std::to_string(metrics.workerProcessCalls) +
           ", workerWaits=" + std::to_string(metrics.workerWaits) +
           ", workerWakeups=" + std::to_string(metrics.workerWakeups) +
           ", workerErrors=" + std::to_string(metrics.workerErrors) +
           ", totalPushed=" + std::to_string(metrics.totalPushed) +
           ", totalPopped=" + std::to_string(metrics.totalPopped) +
           ", encodedPacketsPushed=" + std::to_string(metrics.encodedPacketsPushed) +
           ", encodedPacketsPopped=" + std::to_string(metrics.encodedPacketsPopped) +
           ", backpressureItems=" + std::to_string(backpressure.decisions.size());
}

MediaGraphRuntimeReport MediaGraphRuntimeReporter::capture(const MediaGraphRuntime& runtime)
{
    MediaGraphRuntimeReport report;
    report.state = runtime.state();

    std::size_t queued = 0;
    for (const MediaChannel* channel : runtime.context().channels().channels()) {
        if (channel) {
            queued += channel->size();
            report.metrics.totalPushed += channel->metrics().pushed;
            report.metrics.totalPopped += channel->metrics().popped;
            if (channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
                report.metrics.encodedPacketsPushed += channel->metrics().pushed;
                report.metrics.encodedPacketsPopped += channel->metrics().popped;
            }
        }
    }

    report.metrics.updateQueuedBuffers(queued);
    if (runtime.threadedRunning()) {
        const uint64_t totalPushed = report.metrics.totalPushed;
        const uint64_t totalPopped = report.metrics.totalPopped;
        const uint64_t encodedPacketsPushed = report.metrics.encodedPacketsPushed;
        const uint64_t encodedPacketsPopped = report.metrics.encodedPacketsPopped;
        report.metrics = runtime.threadedExecutor().metrics();
        report.metrics.totalPushed = totalPushed;
        report.metrics.totalPopped = totalPopped;
        report.metrics.encodedPacketsPushed = encodedPacketsPushed;
        report.metrics.encodedPacketsPopped = encodedPacketsPopped;
        report.metrics.updateQueuedBuffers(queued);
    }

    report.backpressure = MediaBackpressureController::inspect(runtime.context());
    return report;
}

} // namespace media::ffmpeg::graph
