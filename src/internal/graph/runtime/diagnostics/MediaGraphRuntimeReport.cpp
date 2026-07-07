#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

std::string MediaGraphRuntimeReport::summary() const
{
    return "runtime report: queued=" + std::to_string(metrics.queuedBuffers) +
           ", peakQueued=" + std::to_string(metrics.peakQueuedBuffers) +
           ", workers=" + std::to_string(metrics.activeWorkers) +
           ", workerErrors=" + std::to_string(metrics.workerErrors) +
           ", totalPushed=" + std::to_string(metrics.totalPushed) +
           ", totalPopped=" + std::to_string(metrics.totalPopped) +
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
        }
    }

    report.metrics.updateQueuedBuffers(queued);
    if (runtime.threadedRunning()) {
        const uint64_t totalPushed = report.metrics.totalPushed;
        const uint64_t totalPopped = report.metrics.totalPopped;
        report.metrics = runtime.threadedExecutor().metrics();
        report.metrics.totalPushed = totalPushed;
        report.metrics.totalPopped = totalPopped;
        report.metrics.updateQueuedBuffers(queued);
    }

    report.backpressure = MediaBackpressureController::inspect(runtime.context());
    return report;
}

} // namespace media::ffmpeg::graph
