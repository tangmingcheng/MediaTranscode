#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"

#include "internal/graph/planner/realtime/MediaCompositionProducerHolderPlanner.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionStartupStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

#include <array>
#include <functional>
#include <utility>

namespace media::ffmpeg::graph {

namespace {
::media::Result<MediaCompositionStartupStoragePlan> sourceStartupStorage(
    const MediaAvSourceCapacityFacts& capacity, const MediaRealtimeRtpInputNodePlan& input,
    const MediaRealtimeAvSourcePlanningProducts& runtime)
{
    using Result = ::media::Result<MediaCompositionStartupStoragePlan>;
    const bool hasAudio = runtime.synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if (!input.rtpTransport || (hasAudio && (!runtime.isolatedAudioInput || !runtime.isolatedAudioInput->rtpTransport)))
        return Result::failure(::media::ErrorInfo::invalidArgument("CNAME route requires input transport facts"));
    const int videoDatagram = input.rtpTransport->maximumDatagramBytes;
    const int audioDatagram = hasAudio ? runtime.isolatedAudioInput->rtpTransport->maximumDatagramBytes : 0;
    if (videoDatagram <= 0 || (hasAudio && audioDatagram <= 0))
        return Result::failure(::media::ErrorInfo::invalidArgument("CNAME route requires positive datagram capacities"));
    const std::array<std::size_t, 2> datagrams{static_cast<std::size_t>(videoDatagram), static_cast<std::size_t>(audioDatagram)};
    return MediaCompositionStartupStoragePlanner::plan(capacity.route,
        capacity.startup.videoUnits, capacity.startup.audioUnits, runtime.groupKey.value().size(),
        std::span<const std::size_t>{datagrams.data(), hasAudio ? 2u : 1u});
}
} // namespace

::media::Result<MediaCompositionSourceFixedProducts>
MediaCompositionSourcePlanningKernel::prepareFixed(MediaCompositionSourceFixedRequest request,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    using Result = ::media::Result<MediaCompositionSourceFixedProducts>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("Source planning requires a preparation resource"));
    const auto& raw = request.raw;
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&request.timing);
    if (cadence && (cadence->rate.num <= 0 || cadence->rate.den <= 0 || cadence->rate != raw.video.frameRate))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition canonical source cadence differs from its observed timing"));
    const auto members = request.sourceRequest.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        hasAudio != raw.audio.has_value() || hasAudio != (request.audioIngress != nullptr) ||
        hasAudio != request.preparedAudioResampler.has_value() ||
        !request.video.decoder.outputFrame || !request.video.decoder.preparedInputRetention ||
        request.sourceRequest.mediaId.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source requires matching members and prepared decoder facts"));
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
    if (request.video.decoderOutputAllocation && !request.video.decoderOutputAllocation->independentOfDecoderSurfaces() &&
        (!request.video.filterActive || !request.video.filterExecution ||
         request.video.filterExecution->output.allocation != MediaVideoFilterAllocation::IndependentOutputPool ||
         request.video.filterExecution->output.completion != MediaVideoFilterCompletion::SynchronousOnReturn))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Borrowed decoder surfaces require the planned independent synchronous tile boundary"));
    if (!cadence) {
        const auto& video = request.video;
        if (!video.decoderOutputAllocation || !video.decoderOwnershipStorage ||
            mediaVideoFrameProducerMaximumExtendedReferences(*video.decoderReadiness) !=
                video.decoderOutputAllocation->maximumExtendedBufferReferences ||
            (video.filterActive && (!video.filterOwnershipStorage ||
                !mediaVideoFrameProducerMaximumExtendedReferences(*video.filterReadiness))))
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Source presentation requires matching producer reference and ownership storage contracts"));
        auto ownership = MediaVideoFrameOwnershipStoragePlanner::plan({
            video.decoderOutputAllocation->maximumExtendedBufferReferences, true,
            std::holds_alternative<MediaCudaFrameProducerFacts>(*video.decoderReadiness)
                ? MediaVideoFrameReadiness::Kind::CudaEvent : MediaVideoFrameReadiness::Kind::Completed});
        if (!ownership) return Result::failure(ownership.error());
        if (*video.decoderOwnershipStorage != ownership.value())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source ownership storage differs from its admitted producer references"));
        if (video.filterActive) {
            auto filterOwnership = MediaVideoFrameOwnershipStoragePlanner::plan({
                *mediaVideoFrameProducerMaximumExtendedReferences(*video.filterReadiness), true,
                std::holds_alternative<MediaCudaFrameProducerFacts>(*video.filterReadiness)
                    ? MediaVideoFrameReadiness::Kind::CudaEvent : MediaVideoFrameReadiness::Kind::Completed});
            if (!filterOwnership) return Result::failure(filterOwnership.error());
            if (*video.filterOwnershipStorage != filterOwnership.value())
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Filter ownership storage differs from its admitted producer references"));
        }
    }
    MediaAudioPipelinePlan audio;
    audio.enabled = hasAudio;
    audio.branchMode = MediaBranchMode::Drop;
    if (hasAudio) {
        auto planned = MediaAudioPipelinePlanner::planSourceForOutput(
            *raw.audio, request.outputAudio, request.preparedAudioResampler);
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
    auto startup = MediaAvSyncStartupPolicyPlanner::prepare(*members,
        cadence ? MediaVideoStartupReadiness::IntervalCoverage : MediaVideoStartupReadiness::DecodedPresentationSpan,
        request.deployment.encode().transportTiming.senderTransportLead);
    if (!startup) return Result::failure(startup.error());
    auto sync = MediaAvSyncPlanner::prepareFixed(request.sourceRequest, nullptr,
        MediaAvSourceContributionDomain{}, nullptr, std::move(startup).value(),
        audio.branchMode, hasAudio ? std::optional{request.outputAudio.sampleRate()} : std::nullopt);
    if (!sync) return Result::failure(sync.error());
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
    auto runtime = MediaRealtimeAvSourceRuntimePlanner::prepareFixed({
        MediaAvSyncGroupKey(request.sourceRequest.mediaId), RealtimeInputType::RtpPort,
        RealtimeInputStreamLayout::SeparateStreams, inputDraft.input, raw.video.streamIndex,
        raw.video.codecName, threading, request.video.filterActive,
        request.activationOutputLead, request.selectedAudio}, std::move(audio),
        std::move(inputDraft.isolatedAudioInput), std::move(sync).value());
    if (!runtime) return Result::failure(runtime.error());
    if (!inputDraft.input.rtpAccessUnitEnvelope ||
        (hasAudio && (!runtime.value().audioPipeline.selectedDecoder->preparedInputRetention ||
            !runtime.value().isolatedAudioInput->rtpAccessUnitEnvelope)))
        return Result::failure(::media::ErrorInfo::notInitialized("Source producer holders require opened codecs and input completion facts"));
    auto plannedHolders = MediaCompositionProducerHolderPlanner::plan(request.video, request.timing,
        *inputDraft.input.rtpAccessUnitEnvelope,
        hasAudio ? &*runtime.value().audioPipeline.selectedDecoder->preparedInputRetention : nullptr,
        hasAudio ? &*runtime.value().isolatedAudioInput->rtpAccessUnitEnvelope : nullptr);
    if (!plannedHolders) return Result::failure(plannedHolders.error());
    auto holders = std::move(plannedHolders).value();
    return Result::success({resource, request.sourceRequest, raw, request.deployment, request.audioRetention,
        request.timing, RealtimeInputType::RtpPort, std::move(inputDraft.input),
        raw.video.streamIndex, request.video, raw.video.frameRate,
        cadence ? std::optional{MediaRational{raw.video.frameRate.den, raw.video.frameRate.num}} : std::nullopt,
        std::move(runtime).value(), std::move(holders)});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourceFixedProducts>::failure(
        ::media::ErrorInfo::allocationFailed("composition fixed source planning"));
}

::media::Result<MediaCompositionSourcePlanningProducts>
MediaCompositionSourcePlanningKernel::bind(MediaCompositionSourceFixedProducts&& fixed, MediaAvSyncCapacityFacts capacity)
try {
    using Result = ::media::Result<MediaCompositionSourcePlanningProducts>;
    const auto& resource = fixed.planningResource;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("Source planning requires a preparation resource"));
    const bool cadence = std::holds_alternative<MediaObservedVideoCadence>(fixed.timing);
    const auto* capacityReference = std::get_if<std::reference_wrapper<const MediaAvSourceCapacityFacts>>(&capacity);
    const auto* sourceCapacity = capacityReference ? &capacityReference->get() : nullptr;
    const auto* ledgerReference = std::get_if<std::reference_wrapper<const MediaRealtimeGraphResourceLedgerPlan>>(&capacity);
    if (cadence != static_cast<bool>(ledgerReference) || (!cadence && !sourceCapacity))
        return Result::failure(::media::ErrorInfo::invalidArgument("Source timing differs from capacity facts domain"));
    const auto members = fixed.runtime.synchronization.members;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    auto startup = std::visit([&](const auto& facts) {
        return MediaAvSyncStartupPolicyPlanner::plan(fixed.sourceRequest, facts.get(), fixed.deployment);
    }, capacity);
    if (!startup) return Result::failure(startup.error());
    if (hasAudio != (fixed.audioRetention != nullptr) ||
        (hasAudio && (!fixed.audioRetention->valid() ||
            startup.value().audioCapacity != fixed.audioRetention->maximumUnits ||
            startup.value().audioByteCapacity != fixed.audioRetention->maximumBytes ||
            startup.value().maximumAudioUnitBytes != fixed.audioRetention->maximumUnitBytes)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition startup audio capacity differs from prepared source retention"));
    const auto& queues = cadence ? ledgerReference->get().queues : sourceCapacity->queues;
    const MediaRealtimeAvSourceClockRequest clockRequest{{fixed.inputType,
        RealtimeInputStreamLayout::SeparateStreams, fixed.input, fixed.videoStreamIndex,
        fixed.raw.video.codecName, fixed.runtime.audioPipeline,
        fixed.runtime.isolatedAudioInput ? &*fixed.runtime.isolatedAudioInput : nullptr}, sourceCapacity};
    auto runtimeCapacity = MediaRealtimeAvSourceRuntimePlanner::planCapacity(
        fixed.runtime, clockRequest, startup.value(), queues);
    if (!runtimeCapacity) return Result::failure(runtimeCapacity.error());
    auto runtime = MediaRealtimeAvSourceRuntimePlanner::bindFacts(
        std::move(fixed.runtime), std::move(runtimeCapacity).value());
    if (!runtime) return Result::failure(runtime.error());
    std::optional<MediaStartupBatchStorageCosts> startupBatchStorage;
    std::optional<MediaStartupRouteStoragePlan> startupRouteStorage;
    std::optional<MediaRtpClockRouteStoragePlan> clockRouteStorage;
    std::optional<MediaStartupCoordinatorStoragePlan> startupCoordinatorStorage;
    if (!cadence) {
        auto storage = sourceStartupStorage(*sourceCapacity, fixed.input, runtime.value());
        if (!storage) return Result::failure(storage.error());
        startupBatchStorage = std::move(storage.value().batch);
        startupRouteStorage = std::move(storage.value().route);
        clockRouteStorage = std::move(storage.value().clock);
        startupCoordinatorStorage = storage.value().coordinator;
    }
    auto branchConnections = MediaCompositionSourceBranchPlanner::plan(
        fixed.timing, fixed.video.filterActive, hasAudio, runtime.value().edgePolicies, resource);
    if (!branchConnections) return Result::failure(branchConnections.error());
    if (!runtime.value().synchronization.sourceClockMode || !runtime.value().synchronization.startup.videoReadiness)
        return Result::failure(::media::ErrorInfo::notInitialized("Source input connections lack clock/readiness facts"));
    auto inputConnections = MediaSourceInputConnectionsPlanner::plan(*members,
        *runtime.value().synchronization.startup.videoReadiness,
        *runtime.value().synchronization.sourceClockMode, runtime.value().edgePolicies, *resource);
    if (!inputConnections) return Result::failure(inputConnections.error());
    return Result::success({resource, fixed.inputType, std::move(fixed.input), fixed.videoStreamIndex,
        fixed.video, fixed.frameRate, fixed.maximumFrameDuplicationGap,
        std::move(runtime).value(), std::move(fixed.producerHolders), std::move(startupBatchStorage),
        std::move(startupRouteStorage), std::move(clockRouteStorage), std::move(startupCoordinatorStorage),
        std::move(branchConnections).value(), std::move(inputConnections).value()});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourcePlanningProducts>::failure(
        ::media::ErrorInfo::allocationFailed("composition source planning catalogs"));
}

::media::Result<MediaCompositionSourcePlanningProducts>
MediaCompositionSourcePlanningKernel::plan(MediaCompositionSourcePlanningRequest request,
    std::shared_ptr<MediaPreparationMemoryResource> resource)
try {
    auto fixed = prepareFixed(std::move(static_cast<MediaCompositionSourceFixedRequest&>(request)), std::move(resource));
    if (!fixed) return ::media::Result<MediaCompositionSourcePlanningProducts>::failure(fixed.error());
    return bind(std::move(fixed).value(), request.capacity);
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourcePlanningProducts>::failure(
        ::media::ErrorInfo::allocationFailed("composition source planning catalogs"));
}

::media::Status MediaCompositionSourcePlanningKernel::rebindCapacity(
    const MediaCompositionSourceFixedRequest& prepared,
    const MediaAvSourceCapacityFacts& capacity, const MediaRealtimeAvSourceTimingFacts& clockTiming,
    MediaCompositionSourcePlanningProducts& products)
{
    const auto members = products.runtime.synchronization.members;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if (!std::holds_alternative<MediaSourcePresentationTiming>(prepared.timing) ||
        products.video.transferDirection != MediaHardwareTransferDirection::None ||
        prepared.sourceRequest.parameters.execution.streamSet != members ||
        hasAudio != (prepared.audioRetention != nullptr) ||
        (hasAudio && (!prepared.audioRetention->valid() ||
            capacity.startup.audioUnits != prepared.audioRetention->maximumUnits ||
            capacity.startup.audioBytes != prepared.audioRetention->maximumBytes ||
            capacity.startup.audioUnitBytes != prepared.audioRetention->maximumUnitBytes)) ||
        !products.startupBatchStorage || !products.startupRouteStorage ||
        !products.clockRouteStorage || !products.startupCoordinatorStorage)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Composition capacity refresh requires fixed prepared source facts"));
    auto startup = MediaAvSyncStartupPolicyPlanner::plan(prepared.sourceRequest, capacity, prepared.deployment);
    if (!startup) return ::media::Status::failure(startup.error());
    const MediaRealtimeAvSourceClockRequest clockRequest{{
        products.inputType, RealtimeInputStreamLayout::SeparateStreams, products.input,
        products.videoStreamIndex, prepared.raw.video.codecName, products.runtime.audioPipeline,
        products.runtime.isolatedAudioInput ? &*products.runtime.isolatedAudioInput : nullptr}, &capacity};
    MediaRealtimeAvSourceCapacityState runtimeCapacity(products.runtime);
    if (auto status = MediaRealtimeAvSourceRuntimePlanner::rebindCapacity(
            clockRequest, clockTiming, startup.value(), runtimeCapacity); !status) return status;
    if (auto status = runtimeCapacity.publish(products.runtime); !status) return status;
    auto storage = sourceStartupStorage(capacity, products.input, products.runtime);
    if (!storage) return ::media::Status::failure(storage.error());
    products.startupBatchStorage = std::move(storage.value().batch);
    products.startupRouteStorage = std::move(storage.value().route);
    products.clockRouteStorage = std::move(storage.value().clock);
    products.startupCoordinatorStorage = storage.value().coordinator;
    const auto& sync = products.runtime.synchronization;
    if (!sync.members || !sync.sourceClockMode || !sync.startup.videoReadiness)
        return ::media::Status::failure(::media::ErrorInfo::notInitialized("Source input connections lack clock/readiness facts"));
    if (auto status = MediaCompositionSourceBranchPlanner::rebind(prepared.timing,
            products.video.filterActive, hasAudio, products.runtime.edgePolicies, products.branchConnections); !status) return status;
    return MediaSourceInputConnectionsPlanner::rebind(*sync.members, *sync.startup.videoReadiness,
        *sync.sourceClockMode, products.runtime.edgePolicies, products.inputConnections);
}

} // namespace media::ffmpeg::graph
