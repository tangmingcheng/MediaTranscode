#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpInputStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceCapacityAdapter.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceRuntimePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmission.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <functional>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeCompositionSourcePlan>
MediaRealtimeCompositionSourcePlanner::plan(MediaRealtimeCompositionSourcePlanningRequest request)
{
    using Result = ::media::Result<MediaRealtimeCompositionSourcePlan>;
    const auto& raw = request.raw;
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&request.timing);
    const auto* sealed = request.prepared.continuousAdmission;
    if (!cadence && (!sealed || !request.prepared.continuousVideoResource ||
        sealed->owner != request.prepared.continuousVideoResource ||
        sealed->owner.owner_before(request.prepared.continuousVideoResource) ||
        request.prepared.continuousVideoResource.owner_before(sealed->owner)))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Source presentation requires its sealed storage owner and capacity"));
    if (cadence && (cadence->rate.num <= 0 || cadence->rate.den <= 0 || cadence->rate != raw.video.frameRate))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition canonical source cadence differs from its observed timing"));
    const auto members = request.sourceRequest.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        hasAudio != raw.audio.has_value() || hasAudio != (request.prepared.audio != nullptr) ||
        hasAudio != (request.prepared.audioIngress != nullptr) ||
        !request.video.decoder.outputFrame || !request.video.decoder.preparedInputRetention ||
        request.sourceRequest.mediaId.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source requires matching sealed members and prepared decoder facts"));
    std::optional<MediaAvSourceCapacityFacts> sourceCapacity;
    if (!cadence) {
        auto facts = MediaPreparedRtpSourceCapacityAdapter::read(
            *sealed, *request.prepared.continuousVideoResource, *members);
        if (!facts) return Result::failure(facts.error());
        sourceCapacity = std::move(facts).value();
    }
    std::optional<MediaRealtimeGraphResourceLedgerPlan> ledger;
    if (cadence) {
        std::vector<MediaRealtimeVideoSurfaceFootprintFact> surfaces;
        const auto addSurface = [&](const MediaHardwareDescriptor& frame) {
            surfaces.push_back({frame.size.width, frame.size.height,
                frame.surfacePixelFormat.empty() ? frame.pixelFormat : frame.surfacePixelFormat,
                raw.video.frameRate, "prepared-composition-source-frame-contract"});
        };
        addSurface(*request.video.decoder.outputFrame);
        if (request.video.filterActive) {
            if (!request.video.filter.inputFrame || !request.video.filter.outputFrame)
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "Composition source filter has no negotiated input/output geometry"));
            addSurface(*request.video.filter.inputFrame);
            addSurface(*request.video.filter.outputFrame);
        }
        // This ledger supplies source queue and startup policy only. Composition's
        // final graph admission uses independently compiled per-producer facts.
        auto plannedLedger = MediaRealtimeGraphResourceLedgerPlanner::plan(
            request.deployment.encode().latency, request.emission, surfaces,
            request.video.decoder.hardware());
        if (!plannedLedger) return Result::failure(plannedLedger.error());
        ledger.emplace(std::move(plannedLedger).value());
    }
    auto storage = MediaPreparedRtpInputStoragePlanner::plan(raw,
        &request.prepared.videoIngress, request.prepared.audioIngress,
        &request.prepared.video, request.prepared.audio);
    if (!storage) return Result::failure(storage.error());
    if (!storage.value().preparation) return Result::failure(::media::ErrorInfo::notInitialized(
        "Composition source has no prepared allocation storage"));
    if (!request.prepared.continuousStorage || !request.prepared.continuousStorage->allocationOwner ||
        !request.prepared.continuousStorage->maximumAllocationBytes ||
        request.prepared.continuousStorage->streamCount != (hasAudio ? 2u : 1u) ||
        request.prepared.continuousStorage->authority.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source has no owned continuous preparation allocation budget"));
    const auto& rawOwner = storage.value().preparation->allocationOwner;
    const auto& continuousOwner = request.prepared.continuousStorage->allocationOwner;
    if (rawOwner.get() == continuousOwner.get() ||
        (!rawOwner.owner_before(continuousOwner) && !continuousOwner.owner_before(rawOwner)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition raw and continuous preparation budgets must retain distinct actual allocations"));
    auto reserved = MediaCheckedArithmetic::add(storage.value().reservedBytes,
        request.prepared.continuousStorage->maximumAllocationBytes,
        "composition raw preparation, ingress arena and continuous session storage");
    if (!reserved) return Result::failure(reserved.error());
    storage.value().reservedBytes = reserved.value();
    storage.value().authority += "+" + request.prepared.continuousStorage->authority;
    std::optional<MediaRealtimeGraphResourceLedgerPlan> admitted;
    if (cadence) {
        auto inputLedger = MediaRealtimeGraphResourceLedgerPlanner::admitPreparedInput(
            std::move(*ledger), std::move(storage.value().payload),
            storage.value().reservedBytes, std::move(storage.value().authority));
        if (!inputLedger) return Result::failure(inputLedger.error());
        auto startup = MediaAvSyncStartupPolicyPlanner::planInputPreflight(request.sourceRequest);
        if (!startup) return Result::failure(startup.error());
        if (!startup.value().maximumWaitNs)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition source startup has no acquisition deadline"));
        if (!request.prepared.continuousRetention ||
            request.prepared.continuousRetention->audio.has_value() != hasAudio)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition source requires its continuous prepared retention contract"));
        if (auto valid = request.prepared.continuousRetention->validate(); !valid)
            return Result::failure(valid.error());
        if (hasAudio != (request.prepared.continuousAudioRetention != nullptr) ||
            (hasAudio && (!request.prepared.continuousAudioRetention->valid() ||
                *request.prepared.continuousAudioRetention != *request.prepared.continuousRetention->audio)))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition startup audio differs from its independent prepared retention"));
        // The source owner and runtime binder retain the same finite AU product.
        // Startup waiting policy remains independently planned above; do not add a
        // second acquisition window to the already admitted prepared prefix.
        auto retainedLedger = MediaRealtimeGraphResourceLedgerPlanner::admitInputRetention(
            std::move(inputLedger).value(), *request.prepared.continuousRetention);
        if (!retainedLedger) return Result::failure(retainedLedger.error());
        admitted.emplace(std::move(retainedLedger).value());
    } else if (hasAudio != (request.prepared.continuousAudioRetention != nullptr) ||
        (hasAudio && (!request.prepared.continuousAudioRetention->valid() ||
            sealed->startup.audioUnits != request.prepared.continuousAudioRetention->maximumUnits ||
            sealed->startup.audioBytes != request.prepared.continuousAudioRetention->maximumBytes ||
            sealed->startup.audioUnitBytes != request.prepared.continuousAudioRetention->maximumUnitBytes)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoded source startup differs from independent audio retention"));
    const MediaAvSyncCapacityFacts capacity = cadence
        ? MediaAvSyncCapacityFacts{std::cref(*admitted)}
        : MediaAvSyncCapacityFacts{std::cref(*sourceCapacity)};
    auto planned = MediaCompositionSourcePlanningKernel::plan({request.sourceRequest, raw, request.timing,
        request.prepared.videoIngress, request.prepared.audioIngress, request.prepared.continuousAudioRetention,
        capacity, std::move(request.video), request.outputAudio, request.deployment,
        request.activationOutputLead, request.selectedAudio});
    if (!planned) return Result::failure(planned.error());
    auto products = std::move(planned).value();
    if (!cadence) {
        if (!sealed->video.bounds.prefixObjects ||
            sealed->video.bounds.prefixObjects != sealed->video.compressed.maximumUnits)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source lineage requires the sealed compressed prefix capacity"));
        const auto sameStream = [](const MediaCompositionStreamHolderFacts& left, const MediaCompositionStreamHolderFacts& right) {
            return left.decoder == right.decoder && left.maximumAtomicInputCompletion == right.maximumAtomicInputCompletion;
        };
        const auto& actual = products.producerHolders;
        const auto& expected = sealed->producerHolders;
        if (!sameStream(actual.video, expected.video) || actual.audio.has_value() != expected.audio.has_value() ||
            (actual.audio && !sameStream(*actual.audio, *expected.audio)) ||
            actual.filterRetainedInputFrames != expected.filterRetainedInputFrames)
            return Result::failure(::media::ErrorInfo::invalidArgument("Composition source producer holders differ from sealed storage admission"));
    }
    MediaLineageAllocationScope allocation = cadence
        ? MediaLineageAllocationScope{MediaRuntimeLineageAllocation{}}
        : MediaLineageAllocationScope{MediaPreparedLineageAllocation{request.prepared.continuousVideoResource}};
    auto reservation = MediaOutputReservationStoragePlanner::plan(std::move(allocation));
    if (!reservation) return Result::failure(reservation.error());
    auto runtime = MediaRealtimeAvSourceRuntimePlanner::bind(std::move(products.runtime), std::move(reservation).value());
    if (!runtime) return Result::failure(runtime.error());
    return Result::success({products.inputType, std::move(products.input), products.videoStreamIndex,
        std::move(products.video), products.frameRate, products.maximumFrameDuplicationGap,
        std::move(runtime).value(), {std::move(*storage.value().preparation), *request.prepared.continuousStorage},
        std::move(products.producerHolders), std::move(products.startupBatchStorage), std::move(products.startupRouteStorage),
        std::move(products.clockRouteStorage), std::move(products.startupStoreStorage),
        std::move(products.branchConnections), std::move(products.inputConnections),
        cadence ? std::nullopt : std::optional<MediaPreparedRtpSourceContainerBounds>{
            {sealed->video.bounds, sealed->audio ? std::optional{sealed->audio->bounds} : std::nullopt}}});
}

} // namespace media::ffmpeg::graph
