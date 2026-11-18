#include "internal/graph/planner/realtime/MediaCompositionSourceRegistryPlanner.h"
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

::media::Result<MediaCompositionSourcePreparedFacts>
MediaRealtimeCompositionSourcePlanner::readPrepared(MediaCompositionSourcePreparedRequest original,
    MediaPreparedRtpSourceView prepared, const MediaPreparedRealtimeEmissionSet& emission)
try {
    using Result = ::media::Result<MediaCompositionSourcePreparedFacts>;
    const auto& raw = original.raw;
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&original.timing);
    const auto* sealed = prepared.continuousAdmission;
    if (!cadence && (!sealed || !prepared.continuousVideoResource ||
        sealed->owner != prepared.continuousVideoResource ||
        sealed->owner.owner_before(prepared.continuousVideoResource) ||
        prepared.continuousVideoResource.owner_before(sealed->owner)))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Source presentation requires its sealed storage owner and capacity"));
    if (cadence && (cadence->rate.num <= 0 || cadence->rate.den <= 0 || cadence->rate != raw.video.frameRate))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition canonical source cadence differs from its observed timing"));
    const auto members = original.sourceRequest.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        hasAudio != raw.audio.has_value() || hasAudio != (prepared.audio != nullptr) ||
        hasAudio != (prepared.audioIngress != nullptr) ||
        !original.video.decoder.outputFrame || !original.video.decoder.preparedInputRetention ||
        original.sourceRequest.mediaId.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source requires matching sealed members and prepared decoder facts"));
    std::optional<MediaAvSourceCapacityFacts> sourceCapacity;
    if (!cadence) {
        auto facts = MediaPreparedRtpSourceCapacityAdapter::read(
            *sealed, *prepared.continuousVideoResource, *members);
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
        addSurface(*original.video.decoder.outputFrame);
        if (original.video.filterActive) {
            if (!original.video.filter.inputFrame || !original.video.filter.outputFrame)
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "Composition source filter has no negotiated input/output geometry"));
            addSurface(*original.video.filter.inputFrame);
            addSurface(*original.video.filter.outputFrame);
        }
        // This ledger supplies source queue and startup policy only. Composition's
        // final graph admission uses independently compiled per-producer facts.
        auto plannedLedger = MediaRealtimeGraphResourceLedgerPlanner::plan(
            original.deployment.encode().latency, emission, surfaces,
            original.video.decoder.hardware());
        if (!plannedLedger) return Result::failure(plannedLedger.error());
        ledger.emplace(std::move(plannedLedger).value());
    }
    auto storage = MediaPreparedRtpInputStoragePlanner::plan(raw,
        &prepared.videoIngress, prepared.audioIngress,
        &prepared.video, prepared.audio);
    if (!storage) return Result::failure(storage.error());
    if (!storage.value().preparation) return Result::failure(::media::ErrorInfo::notInitialized(
        "Composition source has no prepared allocation storage"));
    if (!prepared.continuousStorage || !prepared.continuousStorage->allocationOwner ||
        !prepared.continuousStorage->maximumAllocationBytes ||
        prepared.continuousStorage->streamCount != (hasAudio ? 2u : 1u) ||
        prepared.continuousStorage->authority.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source has no owned continuous preparation allocation budget"));
    const auto& rawOwner = storage.value().preparation->allocationOwner;
    const auto& continuousOwner = prepared.continuousStorage->allocationOwner;
    if (rawOwner.get() == continuousOwner.get() ||
        (!rawOwner.owner_before(continuousOwner) && !continuousOwner.owner_before(rawOwner)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition raw and continuous preparation budgets must retain distinct actual allocations"));
    auto reserved = MediaCheckedArithmetic::add(storage.value().reservedBytes,
        prepared.continuousStorage->maximumAllocationBytes,
        "composition raw preparation, ingress arena and continuous session storage");
    if (!reserved) return Result::failure(reserved.error());
    storage.value().reservedBytes = reserved.value();
    storage.value().authority += "+" + prepared.continuousStorage->authority;
    std::optional<MediaRealtimeGraphResourceLedgerPlan> admitted;
    if (cadence) {
        auto inputLedger = MediaRealtimeGraphResourceLedgerPlanner::admitPreparedInput(
            std::move(*ledger), std::move(storage.value().payload),
            storage.value().reservedBytes, std::move(storage.value().authority));
        if (!inputLedger) return Result::failure(inputLedger.error());
        auto startup = MediaAvSyncStartupPolicyPlanner::planInputPreflight(original.sourceRequest);
        if (!startup) return Result::failure(startup.error());
        if (!startup.value().maximumWaitNs)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition source startup has no acquisition deadline"));
        if (!prepared.continuousRetention ||
            prepared.continuousRetention->audio.has_value() != hasAudio)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition source requires its continuous prepared retention contract"));
        if (auto valid = prepared.continuousRetention->validate(); !valid)
            return Result::failure(valid.error());
        if (hasAudio != (prepared.continuousAudioRetention != nullptr) ||
            (hasAudio && (!prepared.continuousAudioRetention->valid() ||
                *prepared.continuousAudioRetention != *prepared.continuousRetention->audio)))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition startup audio differs from its independent prepared retention"));
        // The source owner and runtime binder retain the same finite AU product.
        // Startup waiting policy remains independently planned above; do not add a
        // second acquisition window to the already admitted prepared prefix.
        auto retainedLedger = MediaRealtimeGraphResourceLedgerPlanner::admitInputRetention(
            std::move(inputLedger).value(), *prepared.continuousRetention);
        if (!retainedLedger) return Result::failure(retainedLedger.error());
        admitted.emplace(std::move(retainedLedger).value());
    } else if (hasAudio != (prepared.continuousAudioRetention != nullptr) ||
        (hasAudio && (!prepared.continuousAudioRetention->valid() ||
            sealed->startup.audioUnits != prepared.continuousAudioRetention->maximumUnits ||
            sealed->startup.audioBytes != prepared.continuousAudioRetention->maximumBytes ||
            sealed->startup.audioUnitBytes != prepared.continuousAudioRetention->maximumUnitBytes)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoded source startup differs from independent audio retention"));
    if (original.audioRetention != prepared.continuousAudioRetention)
        return Result::failure(::media::ErrorInfo::invalidArgument("Sealed source retention differs from its original owner"));
    if (cadence) return Result::success({original, std::move(prepared), std::move(storage).value(), std::move(*admitted)});
    return Result::success({original, std::move(prepared), std::move(storage).value(), std::move(*sourceCapacity)});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionSourcePreparedFacts>::failure(
        ::media::ErrorInfo::allocationFailed("composition sealed source facts"));
}

::media::Result<MediaRealtimeCompositionSourcePlan>
MediaRealtimeCompositionSourcePlanner::bind(MediaCompositionSourcePreparedFacts&& facts,
    MediaCompositionSourcePlanningProducts&& products)
try {
    using Result = ::media::Result<MediaRealtimeCompositionSourcePlan>;
    const auto& prepared = facts.source;
    const auto& original = facts.original;
    const bool cadence = std::holds_alternative<MediaRealtimeGraphResourceLedgerPlan>(facts.capacity);
    const bool hasAudio = original.sourceRequest.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
    const auto* sealed = prepared.continuousAdmission;
    if (!products.planningResource || &products.video != &original.video ||
        products.runtime.synchronization.members != original.sourceRequest.parameters.execution.streamSet ||
        products.runtime.groupKey.value() != original.sourceRequest.mediaId ||
        cadence != std::holds_alternative<MediaObservedVideoCadence>(original.timing) ||
        !facts.storage.preparation || !prepared.continuousStorage ||
        (!cadence && (!sealed || !products.selectedCapacity ||
            !products.selectedCapacity->sameProduct(std::get<MediaAvSourceCapacityFacts>(facts.capacity),
                products.runtime.edgePolicies.metadata, products.runtime.edgePolicies.atomicMetadata))) ||
        (cadence && products.selectedCapacity))
        return Result::failure(::media::ErrorInfo::invalidArgument("Sealed source differs from its selected planning products"));
    auto registry = cadence ? ::media::Result<std::uint64_t>::success(products.runtime.queues.frame)
        : MediaCompositionSourceRegistryPlanner::plan(original.video);
    if (!registry) return Result::failure(registry.error());
    if (products.lineageRegistryCapacity != registry.value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Sealed source registry differs from its prepared opaque facts"));
    // Fixed ingress declarations were copied before freeze. Seal independently
    // reobserved the original candidates; bind exactly the same products.
    const auto* videoTransport = products.input.rtpTransport ? &*products.input.rtpTransport : nullptr;
    const auto* audioTransport = products.runtime.isolatedAudioInput && products.runtime.isolatedAudioInput->rtpTransport
        ? &*products.runtime.isolatedAudioInput->rtpTransport : nullptr;
    if (!videoTransport || !videoTransport->ingress ||
        !videoTransport->ingress->sameProduct(prepared.videoIngress) ||
        hasAudio != (audioTransport != nullptr) ||
        (hasAudio && (!audioTransport->ingress || !audioTransport->ingress->sameProduct(*prepared.audioIngress))) ||
        original.audioRetention != prepared.continuousAudioRetention)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Sealed composition source differs from its fixed preparation"));
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
        : MediaLineageAllocationScope{MediaPreparedLineageAllocation{prepared.continuousVideoResource}};
    auto reservation = MediaOutputReservationStoragePlanner::plan(std::move(allocation));
    if (!reservation) return Result::failure(reservation.error());
    auto runtime = MediaRealtimeAvSourceRuntimePlanner::bind(std::move(products.runtime), std::move(reservation).value());
    if (!runtime) return Result::failure(runtime.error());
    return Result::success({products.fixedStorage, products.planningResource, products.inputType, std::move(products.input), products.videoStreamIndex,
        products.video, products.frameRate, products.maximumFrameDuplicationGap, products.lineageRegistryCapacity,
        std::move(runtime).value(), {std::move(*facts.storage.preparation), *prepared.continuousStorage},
        std::move(products.producerHolders), std::move(products.startupBatchStorage), std::move(products.startupRouteStorage),
        std::move(products.clockRouteStorage), std::move(products.startupCoordinatorStorage),
        std::move(products.branchConnections), std::move(products.inputConnections),
        cadence ? std::nullopt : std::optional<MediaPreparedRtpSourceContainerBounds>{
            {sealed->video.bounds, sealed->audio ? std::optional{sealed->audio->bounds} : std::nullopt}}});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRealtimeCompositionSourcePlan>::failure(
        ::media::ErrorInfo::allocationFailed("composition source planning products"));
}

} // namespace media::ffmpeg::graph
