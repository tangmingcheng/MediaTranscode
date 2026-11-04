#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"

#include "internal/graph/builder/codec/MediaDecoderInputRetentionOptionCodec.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

#include <array>
#include <functional>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaCompositionSourcePlanningProducts>
MediaCompositionSourcePlanningKernel::plan(MediaCompositionSourcePlanningRequest request)
{
    using Result = ::media::Result<MediaCompositionSourcePlanningProducts>;
    const auto& raw = request.raw;
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&request.timing);
    if (cadence && (cadence->rate.num <= 0 || cadence->rate.den <= 0 || cadence->rate != raw.video.frameRate))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition canonical source cadence differs from its observed timing"));
    const auto members = request.sourceRequest.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        hasAudio != raw.audio.has_value() || hasAudio != (request.audioIngress != nullptr) ||
        !request.video.decoder.outputFrame || !request.video.decoder.preparedInputRetention ||
        request.sourceRequest.mediaId.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source requires matching members and prepared decoder facts"));
    const auto* capacityReference = std::get_if<std::reference_wrapper<const MediaAvSourceCapacityFacts>>(&request.capacity);
    const auto* sourceCapacity = capacityReference ? &capacityReference->get() : nullptr;
    const auto* ledgerReference = std::get_if<std::reference_wrapper<const MediaRealtimeGraphResourceLedgerPlan>>(&request.capacity);
    if (static_cast<bool>(cadence) != static_cast<bool>(ledgerReference) || (!cadence && !sourceCapacity))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source timing differs from capacity facts domain"));
    if (!request.video.decoderReadiness || (request.video.filterActive && (!request.video.filterReadiness ||
        !request.video.filterExecution || !request.video.filterExecution->valid() ||
        !request.video.filterExecution->inputConsumption)) ||
        request.video.transferDirection != MediaHardwareTransferDirection::None)
        return Result::failure(::media::ErrorInfo::notInitialized("Composition source producer readiness contract missing"));
    if (request.video.decoderOutputAllocation &&
        (request.video.decoderOutputAllocation->decoder.decoderName != request.video.decoder.ffmpegName ||
         request.video.decoderOutputAllocation->frame != *request.video.decoder.outputFrame))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition decoder output allocation differs from its prepared source"));
    if (!cadence) {
        const auto& video = request.video;
        if (!video.decoderOutputAllocation || !video.decoderOwnershipStorage ||
            video.decoderReadiness->maximumExtendedBufferReferences !=
                video.decoderOutputAllocation->maximumExtendedBufferReferences ||
            (video.filterActive && (!video.filterOwnershipStorage ||
                video.filterReadiness->maximumExtendedBufferReferences !=
                    video.decoderReadiness->maximumExtendedBufferReferences)))
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Source presentation requires matching producer reference and ownership storage contracts"));
        auto ownership = MediaVideoFrameOwnershipStoragePlanner::plan({
            video.decoderOutputAllocation->maximumExtendedBufferReferences, true,
            MediaVideoFrameReadiness::Kind::CudaEvent});
        if (!ownership) return Result::failure(ownership.error());
        if (*video.decoderOwnershipStorage != ownership.value() ||
            (video.filterActive && *video.filterOwnershipStorage != ownership.value()))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source ownership storage differs from its admitted producer references"));
    }
    MediaAudioPipelinePlan audio;
    audio.enabled = hasAudio;
    audio.branchMode = MediaBranchMode::Drop;
    if (hasAudio) {
        auto planned = MediaAudioPipelinePlanner::planSourceForOutput(*raw.audio, request.outputAudio);
        if (!planned) return Result::failure(planned.error());
        audio = std::move(planned).value();
        if (audio.branchMode != MediaBranchMode::TranscodeFrame ||
            !audio.selectedDecoder || !audio.selectedResampler)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition audio contribution requires prepared decoder and resampler"));
    }
    if (hasAudio != (request.audioRetention != nullptr) ||
        (hasAudio && !request.audioRetention->valid()))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source facts require independent audio retention"));
    const auto& queues = cadence ? ledgerReference->get().queues : sourceCapacity->queues;
    auto sync = MediaAvSyncPlanner::plan(request.sourceRequest, nullptr,
        MediaAvSourceContributionDomain{}, nullptr, request.capacity, request.deployment,
        audio.branchMode, hasAudio ? std::optional{request.outputAudio.sampleRate()} : std::nullopt);
    if (!sync) return Result::failure(sync.error());
    if (hasAudio && (sync.value().startup.audioCapacity != request.audioRetention->maximumUnits ||
        sync.value().startup.audioByteCapacity != request.audioRetention->maximumBytes ||
        sync.value().startup.maximumAudioUnitBytes != request.audioRetention->maximumUnitBytes))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition startup audio capacity differs from prepared source retention"));
    MediaRealtimeRtpTranscodePlanningDraft inputDraft;
    MediaRealtimeInputPlanner::applyNodePlans(request.sourceRequest, &raw, inputDraft);
    if (!inputDraft.input.rtpTransport || hasAudio != inputDraft.isolatedAudioInput.has_value() ||
        (hasAudio && !inputDraft.isolatedAudioInput->rtpTransport))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source has incomplete RTP input transport products"));
    inputDraft.input.requiresPreparedInput = true;
    if (auto status = MediaPreparedRtpIngressPlanner::bind(
            request.videoIngress, *inputDraft.input.rtpTransport); !status)
        return Result::failure(status.error());
    if (hasAudio) {
        inputDraft.isolatedAudioInput->requiresPreparedInput = true;
        if (auto status = MediaPreparedRtpIngressPlanner::bind(
            *request.audioIngress, *inputDraft.isolatedAudioInput->rtpTransport); !status)
            return Result::failure(status.error());
    }
    MediaThreadingPolicy threading;
    threading.mode = MediaThreadingMode::PerNodeWorker;
    threading.priority = MediaThreadPriority::High;
    threading.collectWorkerMetrics = true;
    auto runtime = MediaRealtimeAvSourceRuntimePlanner::planFacts({
        MediaAvSyncGroupKey(request.sourceRequest.mediaId), RealtimeInputType::RtpPort,
        RealtimeInputStreamLayout::SeparateStreams, inputDraft.input, raw.video.streamIndex,
        raw.video.codecName, queues, threading, request.video.filterActive,
        request.activationOutputLead, request.selectedAudio, sourceCapacity}, std::move(audio),
        std::move(inputDraft.isolatedAudioInput), std::move(sync).value());
    if (!runtime) return Result::failure(runtime.error());
    if (!inputDraft.input.rtpAccessUnitEnvelope ||
        (hasAudio && (!runtime.value().audioPipeline.selectedDecoder->preparedInputRetention ||
            !runtime.value().isolatedAudioInput->rtpAccessUnitEnvelope)))
        return Result::failure(::media::ErrorInfo::notInitialized("Source producer holders require opened codecs and input completion facts"));
    MediaCompositionProducerHolderFacts holders{
        {*request.video.decoder.preparedInputRetention,
         inputDraft.input.rtpAccessUnitEnvelope->asInputPayloadEnvelope().maximumPayloadsPerInputCompletion}, {}, {}};
    if (hasAudio) holders.audio.emplace(MediaCompositionStreamHolderFacts{
        *runtime.value().audioPipeline.selectedDecoder->preparedInputRetention,
        runtime.value().isolatedAudioInput->rtpAccessUnitEnvelope->asInputPayloadEnvelope().maximumPayloadsPerInputCompletion});
    if (auto valid = MediaDecoderInputRetentionOptionCodec::encode(holders.video.decoder); !valid)
        return Result::failure(valid.error());
    if (holders.audio) {
        if (auto valid = MediaDecoderInputRetentionOptionCodec::encode(holders.audio->decoder); !valid)
            return Result::failure(valid.error());
    }
    if (!holders.video.maximumAtomicInputCompletion || (holders.audio && !holders.audio->maximumAtomicInputCompletion))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source producer holders require finite atomic completion bounds"));
    if (request.video.filterActive) {
        const auto& execution = *request.video.filterExecution;
        if (execution.output.filterDescription != request.video.filter.filterName ||
            execution.frameProduct != (cadence ? MediaVideoFrameLineageProduct::CanonicalInterval
                : MediaVideoFrameLineageProduct::SourceDisplayPoint))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source filter retention differs from its planned execution"));
        holders.filterRetainedInputFrames = execution.output.maximumRetainedInputFrames;
    }
    std::optional<MediaStartupBatchStorageCosts> startupBatchStorage;
    std::optional<MediaStartupRouteStoragePlan> startupRouteStorage;
    std::optional<MediaRtpClockRouteStoragePlan> clockRouteStorage;
    if (!cadence) {
        auto reservationLayout = MediaOutputReservationStoragePlanner::layout();
        if (!reservationLayout) return Result::failure(reservationLayout.error());
        auto batchStorage = MediaStartupBatchStoragePlanner::plan(sourceCapacity->route,
            sourceCapacity->startup.videoUnits, sourceCapacity->startup.audioUnits,
            reservationLayout.value(), runtime.value().groupKey.value().size());
        if (!batchStorage) return Result::failure(batchStorage.error());
        startupBatchStorage = std::move(batchStorage).value();
        auto routeCosts = MediaStartupRouteStoragePlanner::plan(sourceCapacity->route,
            sourceCapacity->startup.videoUnits, sourceCapacity->startup.audioUnits, *startupBatchStorage);
        if (!routeCosts) return Result::failure(routeCosts.error());
        startupRouteStorage = std::move(routeCosts).value();
        const int videoDatagram = inputDraft.input.rtpTransport->maximumDatagramBytes;
        const int audioDatagram = hasAudio ? runtime.value().isolatedAudioInput->rtpTransport->maximumDatagramBytes : 0;
        if (videoDatagram <= 0 || (hasAudio && audioDatagram <= 0))
            return Result::failure(::media::ErrorInfo::invalidArgument("CNAME route requires positive datagram capacities"));
        const std::array<std::size_t, 2> datagrams{static_cast<std::size_t>(videoDatagram), static_cast<std::size_t>(audioDatagram)};
        auto routeStorage = MediaRtpClockRouteStoragePlanner::plan(*members,
            runtime.value().edgePolicies.metadata,
            std::span<const std::size_t>{datagrams.data(), hasAudio ? 2u : 1u});
        if (!routeStorage) return Result::failure(routeStorage.error());
        clockRouteStorage = std::move(routeStorage).value();
    }
    auto branchConnections = MediaCompositionSourceBranchPlanner::plan(
        request.timing, request.video.filterActive, hasAudio, runtime.value().edgePolicies);
    if (!branchConnections) return Result::failure(branchConnections.error());
    if (!runtime.value().synchronization.sourceClockMode || !runtime.value().synchronization.startup.videoReadiness)
        return Result::failure(::media::ErrorInfo::notInitialized("Source input connections lack clock/readiness facts"));
    auto inputConnections = MediaSourceInputConnectionsPlanner::plan(*members,
        *runtime.value().synchronization.startup.videoReadiness,
        *runtime.value().synchronization.sourceClockMode, runtime.value().edgePolicies);
    if (!inputConnections) return Result::failure(inputConnections.error());
    return Result::success({RealtimeInputType::RtpPort, std::move(inputDraft.input),
        raw.video.streamIndex, std::move(request.video), raw.video.frameRate,
        cadence ? std::optional{MediaRational{raw.video.frameRate.den, raw.video.frameRate.num}} : std::nullopt,
        std::move(runtime).value(), std::move(holders), std::move(startupBatchStorage),
        std::move(startupRouteStorage), std::move(clockRouteStorage), std::move(branchConnections).value(), std::move(inputConnections).value()});
}

} // namespace media::ffmpeg::graph
