#include "internal/graph/runtime/MediaGraphRuntimeReport.h"

namespace media::ffmpeg::graph {

std::string MediaGraphRuntimeReport::summary() const
{
    return "runtime report: queued=" + std::to_string(metrics.queuedBuffers) +
           ", peakQueued=" + std::to_string(metrics.peakQueuedBuffers) +
           ", workers=" + std::to_string(metrics.activeWorkers) +
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
        }
    }

    report.metrics.updateQueuedBuffers(queued);
    if (runtime.threadedRunning()) {
        report.metrics = runtime.threadedExecutor().metrics();
        report.metrics.updateQueuedBuffers(queued);
    }

    report.backpressure = MediaBackpressureController::inspect(runtime.context());
    return report;
}

} // namespace media::ffmpeg::graph
