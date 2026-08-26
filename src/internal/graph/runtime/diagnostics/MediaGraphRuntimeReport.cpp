#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"

#include <algorithm>

namespace media::ffmpeg::graph {

std::string MediaGraphRuntimeReport::summary() const
{
    std::string result =
           "runtime report: queued=" + std::to_string(metrics.queuedBuffers) +
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
           ", logicalProcessors=" + std::to_string(metrics.logicalProcessorCount) +
           ", averageSystemMachineCpuPercent=" + std::to_string(metrics.averageSystemMachineCpuPercent) +
           ", averageProcessMachineCpuPercent=" + std::to_string(metrics.averageProcessMachineCpuPercent) +
           ", peakProcessMachineCpuPercent=" + std::to_string(metrics.peakProcessMachineCpuPercent) +
           ", averageProcessSingleCoreCpuPercent=" + std::to_string(metrics.averageProcessSingleCoreCpuPercent) +
           ", peakProcessSingleCoreCpuPercent=" + std::to_string(metrics.peakProcessSingleCoreCpuPercent) +
           ", initialWorkingSetBytes=" + std::to_string(metrics.initialWorkingSetBytes) +
           ", workingSetBytes=" + std::to_string(metrics.workingSetBytes) +
           ", peakWorkingSetBytes=" + std::to_string(metrics.peakWorkingSetBytes) +
           ", totalPushed=" + std::to_string(metrics.totalPushed) +
           ", totalPopped=" + std::to_string(metrics.totalPopped) +
           ", droppedBuffers=" + std::to_string(metrics.droppedBuffers) +
           ", encodedPacketsPushed=" + std::to_string(metrics.encodedPacketsPushed) +
           ", encodedPacketsPopped=" + std::to_string(metrics.encodedPacketsPopped) +
           ", backpressureItems=" + std::to_string(backpressure.decisions.size());
    if (payloadCredits) {
        result +=
            ", graphPayloadCurrentBytes=" +
                std::to_string(payloadCredits->currentBytes) +
            ", graphPayloadHighWaterBytes=" +
                std::to_string(payloadCredits->highWaterBytes) +
            ", graphPayloadCurrentObjects=" +
                std::to_string(payloadCredits->currentObjects) +
            ", graphPayloadHighWaterObjects=" +
                std::to_string(payloadCredits->highWaterObjects) +
            ", graphPayloadReservations=" +
                std::to_string(payloadCredits->reservations) +
            ", graphPayloadReleases=" +
                std::to_string(payloadCredits->releases) +
            ", graphPayloadPressureFailures=" +
                std::to_string(payloadCredits->pressureFailures);
    }
    if (!droppedEdges.empty()) {
        result += ", droppedEdges=";
        for (std::size_t index = 0; index < droppedEdges.size(); ++index) {
            if (index != 0) result += "|";
            result += std::to_string(droppedEdges[index].edgeId.value) + ":" +
                std::to_string(droppedEdges[index].droppedBuffers);
        }
    }
    return result;
}

MediaGraphRuntimeReport MediaGraphRuntimeReporter::capture(MediaGraphRuntime& runtime)
{
    return capture(static_cast<const MediaGraphRuntime&>(runtime));
}

MediaGraphRuntimeReport MediaGraphRuntimeReporter::capture(const MediaGraphRuntime& runtime)
{
    MediaGraphRuntimeReport report;
    report.state = runtime.state();
    const MediaGraphRuntimeMetrics acceptance = runtime.acceptanceCollector().snapshot();
    report.metrics.cpuSampleCount = acceptance.cpuSampleCount;
    report.metrics.averageSystemMachineCpuPercent =
        acceptance.averageSystemMachineCpuPercent;
    report.metrics.stalledIntervals = acceptance.stalledIntervals;
    report.metrics.errorCount = acceptance.errorCount;
    report.metrics.averageProcessMachineCpuPercent =
        acceptance.averageProcessMachineCpuPercent;
    report.metrics.peakProcessMachineCpuPercent =
        acceptance.peakProcessMachineCpuPercent;
    report.metrics.averageProcessSingleCoreCpuPercent =
        acceptance.averageProcessSingleCoreCpuPercent;
    report.metrics.peakProcessSingleCoreCpuPercent =
        acceptance.peakProcessSingleCoreCpuPercent;
    report.metrics.logicalProcessorCount = acceptance.logicalProcessorCount;
    report.metrics.initialWorkingSetBytes = acceptance.initialWorkingSetBytes;
    report.metrics.workingSetBytes = acceptance.workingSetBytes;
    report.metrics.peakWorkingSetBytes = acceptance.peakWorkingSetBytes;
    report.metrics.processThreadCount = acceptance.processThreadCount;

    std::size_t queued = 0;
    std::size_t channelQueuePeaks = 0;
    for (const MediaChannel* channel : runtime.context().channels().channels()) {
        if (channel) {
            queued += channel->size();
            channelQueuePeaks += channel->metrics().queue.peakSize;
            report.metrics.totalPushed += channel->metrics().pushed;
            report.metrics.totalPopped += channel->metrics().popped;
            report.metrics.droppedBuffers += channel->metrics().queue.dropped;
            if (channel->metrics().queue.dropped != 0) {
                report.droppedEdges.push_back(
                    MediaDroppedEdgeReport{
                        channel->edgeId(),
                        channel->metrics().queue.dropped});
            }
            if (channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
                report.metrics.encodedPacketsPushed += channel->metrics().pushed;
                report.metrics.encodedPacketsPopped += channel->metrics().popped;
            }
        }
    }

    const std::size_t graphQueuePeak = std::max(
        runtime.observeQueueHighWatermark(queued), channelQueuePeaks);
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
        report.metrics.averageSystemMachineCpuPercent =
            acceptance.averageSystemMachineCpuPercent;
        report.metrics.stalledIntervals = acceptance.stalledIntervals;
        report.metrics.errorCount += acceptance.errorCount;
        report.metrics.totalPushed = totalPushed;
        report.metrics.totalPopped = totalPopped;
        report.metrics.droppedBuffers = droppedBuffers;
        report.metrics.encodedPacketsPushed = encodedPacketsPushed;
        report.metrics.encodedPacketsPopped = encodedPacketsPopped;
        report.metrics.averageProcessMachineCpuPercent =
            acceptance.averageProcessMachineCpuPercent;
        report.metrics.peakProcessMachineCpuPercent =
            acceptance.peakProcessMachineCpuPercent;
        report.metrics.averageProcessSingleCoreCpuPercent =
            acceptance.averageProcessSingleCoreCpuPercent;
        report.metrics.peakProcessSingleCoreCpuPercent =
            acceptance.peakProcessSingleCoreCpuPercent;
        report.metrics.logicalProcessorCount = acceptance.logicalProcessorCount;
        report.metrics.initialWorkingSetBytes = acceptance.initialWorkingSetBytes;
        report.metrics.workingSetBytes = acceptance.workingSetBytes;
        report.metrics.peakWorkingSetBytes = acceptance.peakWorkingSetBytes;
        report.metrics.processThreadCount = acceptance.processThreadCount;
        report.metrics.updateQueuedBuffers(queued, graphQueuePeak);
    }

    report.backpressure = MediaBackpressureController::inspect(runtime.context());
    if (const auto ledger = runtime.context().payloadCreditLedger()) {
        report.payloadCredits = ledger->snapshot();
    }
    return report;
}

} // namespace media::ffmpeg::graph
