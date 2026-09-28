#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvProtocolOutputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeAvOutputRuntimePlan> MediaRealtimeAvOutputRuntimePlanner::plan(
    const MediaRealtimeAvOutputRuntimeRequest& request,
    MediaAvSyncPlan synchronization,
    MediaRealtimeOutputPlanningDraft& output)
{
    using Result = ::media::Result<MediaRealtimeAvOutputRuntimePlan>;
    if (auto status = MediaAvSyncPlanValidator::validateDomain(
            synchronization, MediaAvSyncDomainRole::ContinuousOutput); !status)
        return Result::failure(status.error());
    if (!request.timing.outputSampleRate ||
        *request.timing.outputSampleRate != request.audio.sampleRate() ||
        !request.timing.protocolBatchSamples || *request.timing.protocolBatchSamples <= 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output runtime requires matching resolved audio timing"));

    auto selectedTiming = MediaRealtimeAvSyncPlanningFactsResolver::resolveOutput(
        request.audio, output, synchronization);
    if (!selectedTiming) return Result::failure(selectedTiming.error());
    if (selectedTiming.value() != request.timing)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output runtime timing must match the resolved protocol products"));

    std::optional<MediaAudioEncoderFifoRetentionPlan> fifo;
    if (request.audio.branchMode() == MediaBranchMode::TranscodeFrame) {
        if (!request.maximumInputAudioSamples)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Output audio encoder requires its upstream block bound"));
        auto selected = MediaAudioEncoderFifoRetentionPlan::create(
            request.audio, *request.maximumInputAudioSamples);
        if (!selected) return Result::failure(selected.error());
        fifo = std::move(selected).value();
    } else if (request.audio.branchMode() != MediaBranchMode::CopyPacket ||
               request.maximumInputAudioSamples) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output runtime requires frame encoding or packet copy without an encoder FIFO"));
    }

    auto protocol = MediaRealtimeAvProtocolOutputPlanner::plan(
        {request.groupKey, request.layout, request.transport,
         synchronization.rtpOutput, synchronization.projectMpegTsOutput,
         synchronization.startup.outputLeadNs, request.videoFrameRate,
         request.timing.outputSampleRate, request.timing.protocolBatchSamples,
         request.edgePolicies.synchronizedPacket.bufferPolicy.memoryBudget.maxBytes,
         request.deployment, request.emission}, output);
    if (!protocol) return Result::failure(protocol.error());
    auto selected = std::move(protocol).value();
    return Result::success({request.audio, request.groupKey, std::move(synchronization),
        request.queues, request.edgePolicies, selected.activationOutputLead, selected.adapter,
        std::move(selected.protocolOutput), std::move(selected.datagramTransport), std::move(fifo)});
}

} // namespace media::ffmpeg::graph
