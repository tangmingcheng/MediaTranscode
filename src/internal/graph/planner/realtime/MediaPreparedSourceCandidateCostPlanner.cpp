#include "internal/graph/planner/realtime/MediaCompositionSourceRegistryPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedSourceCandidateCostPlanner.h"
#include <new>
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedPacketStorage.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <initializer_list>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {
using Bytes = ::media::Result<std::size_t>;
Bytes checked(::media::Result<std::uint64_t> value)
{
    if (!value) return Bytes::failure(value.error());
    if (value.value() > (std::numeric_limits<std::size_t>::max)())
        return Bytes::failure(::media::ErrorInfo::allocationFailed("Candidate storage exceeds address space"));
    return Bytes::success(static_cast<std::size_t>(value.value()));
}
Bytes add(std::size_t a, std::size_t b)
{ return checked(MediaCheckedArithmetic::add(a, b, "Candidate storage sum")); }
Bytes copies(const MediaSharedAllocationGeometry& geometry, std::size_t count)
{
    auto unit = MediaPreparationMemoryResource::allocationFootprint(geometry.bytes, geometry.alignment);
    if (!unit) return unit;
    return checked(MediaCheckedArithmetic::multiply(unit.value(), count, "Candidate shared owners"));
}
::media::Status validatePacketProgress(const MediaPreparedPacketWrapperStoragePlan& catalog,
    const MediaPreparedSourceCandidateCostFacts& source, const MediaPreparedRtpContainerBounds& containers)
{
    using Kind = MediaPreparedPacketWrapperRootKind;
    if (catalog.sourceIndex != source.sourceIndex ||
        (catalog.stream != MediaStreamKind::Video && catalog.stream != MediaStreamKind::Audio) ||
        (catalog.stream == MediaStreamKind::Audio && !source.startupAudioUnits))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Candidate packet progress has inconsistent source facts"));
    bool rawSeen = false, binderSeen = false;
    // Every catalog creator has one serial Construction root. It identifies
    // the producer but is deliberately excluded from graph reachable retention.
    for (const auto& marker : catalog.roots) {
        if (marker.kind != Kind::Construction) continue;
        if (marker.creatorPosition >= source.topology.positions.size())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Candidate packet progress has an invalid creator"));
        const auto& producer = source.topology.positions[marker.creatorPosition];
        auto* seen = producer.kind == MediaNodeKind::RawRtpInput ? &rawSeen
            : producer.kind == MediaNodeKind::RtpPacketClockBinder ? &binderSeen : nullptr;
        if (!seen || *seen || producer.source != source.sourceIndex)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Candidate packet progress creator is absent or repeated"));
        *seen = true;
        std::uint64_t reachable = 0;
        for (const auto& root : catalog.roots) {
            if (root.creatorPosition != marker.creatorPosition || root.kind != Kind::GraphHolder) continue;
            if (!root.holder || root.maximumWrappers != root.holder->maximumReferences)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Candidate packet progress holder count differs from its catalog"));
            auto sum = MediaCheckedArithmetic::add(reachable, root.maximumWrappers, "Candidate packet graph residence");
            if (!sum) return ::media::Status::failure(sum.error());
            reachable = sum.value();
        }
        // Decoded video startup retains frames, not compressed packet slots.
        // Audio continues to retain its separately planned startup packets.
        const MediaCompositionPacketResidenceCapacity capacity{
            catalog.stream == MediaStreamKind::Video ? 0 : *source.startupAudioUnits,
            containers.acquiringObjects};
        auto residence = MediaCompositionProducerResidencePlanner::plan(source.producerHolders,
            producer.kind, catalog.stream, MediaPayloadKind::Packet, capacity);
        if (!residence) return ::media::Status::failure(residence.error());
        auto total = MediaCheckedArithmetic::add(reachable, residence.value().additionalRetainedObjects,
            "Candidate packet complete residence");
        if (!total) return ::media::Status::failure(total.error());
        if (total.value() < residence.value().minimumProgressObjects)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Candidate packet progress deficit: source=" + std::to_string(source.sourceIndex) +
                " creator=" + std::to_string(marker.creatorPosition) +
                " stream=" + (catalog.stream == MediaStreamKind::Video ? "video" : "audio") +
                " residence=" + std::to_string(total.value()) +
                " required=" + std::to_string(residence.value().minimumProgressObjects)));
    }
    if (!rawSeen || !binderSeen)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Candidate packet progress lacks raw input or clock binder"));
    return ::media::Status::success();
}
template<bool Updating> ::media::Status streamCosts(
    const MediaPreparedRtpSourceSessionStreamPlan& stream,
    const MediaPreparedRtpStreamStorageSnapshot& held,
    const MediaPreparedStreamCandidateCostFacts& facts, bool snapshot,
    const MediaPreparedSourceCandidateCostFacts& source, MediaStreamKind streamKind,
    MediaPreparedStreamCandidateCosts& costs)
{
    using R = ::media::Status;
    if (auto valid = stream.envelope.validate(); !valid) return R::failure(valid.error());
    auto floor = MediaPreparedRtpSourceStoragePlanner::minimumBounds(stream, held);
    if (!floor) return R::failure(floor.error());
    if (stream.envelope.streamKind != streamKind)
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate payload envelope has the wrong stream role"));
    if (facts.containers.prefixObjects < floor.value().prefixObjects ||
        facts.containers.acquiringObjects < floor.value().acquiringObjects ||
        facts.containers.outputObjects < floor.value().outputObjects)
        return R::failure(::media::ErrorInfo::invalidArgument(
            "Candidate cannot discard live preparation owners or serialized completion capacity"));
    auto candidate = MediaPreparedRtpSourceStoragePlanner::containers(stream, facts.containers, snapshot);
    auto history = MediaPreparedRtpSourceStoragePlanner::containers(stream, held.history, snapshot);
    if (!candidate) return R::failure(candidate.error());
    if (!history) return R::failure(history.error());
    auto& result = candidate.value();
    constexpr auto fields = std::array{
        &MediaPreparedRtpContainerStoragePlan::outputPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::acquiringPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::prefixPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::pendingPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::pendingLeaseControlBytes,
        &MediaPreparedRtpContainerStoragePlan::snapshotBytes,
        &MediaPreparedRtpContainerStoragePlan::protocolPendingPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::protocolRequestPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::completionPeakBytes,
        &MediaPreparedRtpContainerStoragePlan::aacHeaderPeakBytes};
    for (auto field : fields) result.*field = (std::max)(result.*field, history.value().*field);
    // Session pending starts empty and only push_back/clear are used; its future
    // growth follows the same from-zero recurrence in containers(). Existing
    // backing survives clear and is also checked against the actual snapshot.
    if (held.pendingCapacity) {
        using Reference = std::shared_ptr<MediaPreparationStorageLease>;
        auto size = checked(MediaCheckedArithmetic::multiply(held.pendingCapacity, sizeof(Reference), "Held pending backing"));
        if (!size) return R::failure(size.error());
        auto backing = MediaPreparationMemoryResource::allocationFootprint(size.value(), alignof(Reference));
        auto proxy = MediaPmrContainerGeometry::proxyBytes();
        if (!backing) return R::failure(backing.error());
        if (!proxy) return R::failure(proxy.error());
        auto total = add(backing.value(), proxy.value());
        if (!total) return R::failure(total.error());
        result.pendingPeakBytes = (std::max)(result.pendingPeakBytes, total.value());
    }
    result.peakBytes = 0;
    for (auto field : fields) {
        auto total = add(result.peakBytes, result.*field);
        if (!total) return R::failure(total.error());
        result.peakBytes = total.value();
    }
    auto lease = probeMediaSharedAllocationGeometry<MediaPreparationStorageLease>(
        std::type_identity<std::pmr::polymorphic_allocator<MediaPreparationStorageLease>>{});
    if (!lease) return R::failure(lease.error());
    using Root = MediaPreparedPayloadLeaseRoot;
    using Kind = MediaPreparedPayloadLeaseRootKind;
    const std::array roots{
        Root{Kind::Prefix, facts.containers.prefixObjects},
        Root{Kind::Acquiring, facts.containers.acquiringObjects},
        Root{Kind::Outputs, facts.containers.outputObjects},
        Root{Kind::ProtocolPending, stream.envelope.maximumAccessUnitsPerPush},
        Root{Kind::RawInputCreditWait, 1}, // RawRtpInputNode::m_preparedPendingPacket
        Root{Kind::BinderCreditWait, 1}}; // RtpPacketClockBinderNode::m_preparedPendingCredit
    // Protocol construction is inside the bounded pending batch. Before freeze,
    // binding drains acquiring serially under the session mutex, so its removed
    // slot covers the in-flight control until prefix publication. Snapshot copies
    // alias the same monotonic prefix. Moves and wrapper replacement create no
    // new control; graph-credit attachment ends its preparation lifetime.
    std::size_t liveControls = 0;
    for (const auto& root : roots) {
        auto total = add(liveControls, root.maximumControls);
        if (!total) return R::failure(total.error());
        liveControls = total.value();
    }
    auto leases = copies(lease.value(), liveControls);
    if (!leases) return R::failure(leases.error());
    if constexpr (Updating) {
        if (!costs.packetWrappers || costs.packetWrappers->sourceIndex != source.sourceIndex || costs.packetWrappers->stream != streamKind)
            return R::failure(::media::ErrorInfo::invalidArgument("Candidate packet catalog changed source or stream"));
        if (auto status = MediaPreparedPacketWrapperStoragePlanner::rebind(*costs.packetWrappers,
                source.topology, source.residence, facts.containers, stream.envelope.maximumAccessUnitsPerPush); !status)
            return status;
    } else {
        auto wrappers = MediaPreparedPacketWrapperStoragePlanner::plan(source.topology, source.residence,
            source.sourceIndex, streamKind, facts.containers, stream.envelope.maximumAccessUnitsPerPush);
        if (!wrappers) return R::failure(wrappers.error());
        costs.packetWrappers.emplace(std::move(wrappers).value());
    }
    if (auto status = validatePacketProgress(*costs.packetWrappers, source, facts.containers); !status) return status;
    auto packet = preparedMediaPacketStorageBytes(stream.envelope.maximumAccessUnitBytes,
        MediaPacketStorageExtent::Payload);
    if (!packet) return R::failure(packet.error());
    auto payload = checked(MediaCheckedArithmetic::multiply(packet.value(), liveControls,
        "Candidate live preparation payload reservations"));
    if (!payload) return R::failure(payload.error());
    // The same roots bound live leases and their retained payload reservation.
    // Fragment assembly is already in the ordinary protocol storage component.
    // Snapshot aliases do not create another payload; graph-credit attachment
    // ends the preparation lease, so graph-owned payload is a separate domain.
    costs.containers = result;
    costs.containerBytes = result.peakBytes - result.pendingLeaseControlBytes;
    costs.leaseControlsBytes = leases.value();
    costs.leaseControlRoots = roots;
    costs.payload = {liveControls, packet.value(), payload.value()};
    return R::success();
}

template<bool Updating> ::media::Status computeCosts(
    const MediaPreparedRtpSourceSessionPlan& session, const MediaPreparedRtpStorageSnapshot& held,
    const MediaPreparedSourceCandidateCostFacts& facts, MediaPreparedSourceCandidateCosts& result)
{
    using R = ::media::Status;
    const bool audio = session.audio.has_value();
    if (session.canonicalRuntimeRetention || !session.fixedOwnerAllocationBytes || !held.owner ||
        audio != held.audio.has_value() || audio != facts.audio.has_value() ||
        audio != facts.startupAudioUnits.has_value() || !facts.startupVideoUnits ||
        facts.startupVideoUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        (audio && (!*facts.startupAudioUnits || *facts.startupAudioUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()))))
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate costs require prepared source member facts"));
    auto registry = MediaCompositionSourceRegistryPlanner::plan(facts.videoPlan);
    if (!registry) return R::failure(registry.error());
    if (facts.registryCapacity != registry.value())
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate registry capacity differs from prepared opaque retention"));
    auto fixed = add(session.fixedStorageBytes, *session.fixedOwnerAllocationBytes);
    if (!fixed) return R::failure(fixed.error());
    fixed = add(fixed.value(), session.video.unmigratedProtocolStorage.totalBytes);
    if (!fixed) return R::failure(fixed.error());
    if (audio) { fixed = add(fixed.value(), session.audio->unmigratedProtocolStorage.totalBytes); if (!fixed) return R::failure(fixed.error()); }
    if (facts.videoPlan.transferDirection != MediaHardwareTransferDirection::None)
        return R::failure(::media::ErrorInfo::unsupported(
            "Source lineage requires a proven passthrough transfer contract"));
    if constexpr (Updating) {
        if (result.audio.has_value() != audio || !result.lineage || result.lineage->sourceIndex != facts.sourceIndex)
            return R::failure(::media::ErrorInfo::invalidArgument("Candidate cost catalog membership changed"));
    } else if (audio) result.audio.emplace();
    if (auto status = streamCosts<Updating>(session.video, held.video, facts.video, true,
            facts, MediaStreamKind::Video, result.video); !status) return status;
    if (audio) {
        if (auto status = streamCosts<Updating>(*session.audio, *held.audio, *facts.audio, false,
                facts, MediaStreamKind::Audio, *result.audio); !status) return status;
    }
    auto index = MediaAvStartupIndexStoragePlanner::preparedIndex(facts.startupVideoUnits,
        MediaVideoStartupReadiness::DecodedPresentationSpan);
    auto generation = MediaAvStartupIndexStoragePlanner::preparedGeneration(facts.startupVideoUnits,
        facts.startupAudioUnits.value_or(0));
    if (!index) return R::failure(index.error());
    if (!generation) return R::failure(generation.error());
    auto indexBytes = index.value().allocationBytes;
    if (audio) {
        auto audioIndex = MediaAvStartupIndexStoragePlanner::preparedIndex(*facts.startupAudioUnits,
            MediaVideoStartupReadiness::IntervalCoverage);
        if (!audioIndex) return R::failure(audioIndex.error());
        auto combined = add(indexBytes, audioIndex.value().allocationBytes);
        if (!combined) return R::failure(combined.error());
        indexBytes = combined.value();
    }
    if (!facts.videoPlan.decoderReadiness || facts.videoPlan.filterActive != facts.videoPlan.filterReadiness.has_value())
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate frame wrapper readiness membership is incomplete"));
    if constexpr (Updating) {
        if (auto status = MediaSourceLineageStoragePlanner::rebind(*result.lineage, facts.topology,
                facts.residence, facts.registryCapacity, facts.video.containers.prefixObjects); !status) return status;
    } else {
        auto lineage = MediaSourceLineageStoragePlanner::plan(facts.topology, facts.residence,
            facts.sourceIndex, facts.registryCapacity, facts.video.containers.prefixObjects, facts.videoPlan.transferDirection);
        if (!lineage) return R::failure(lineage.error());
        result.lineage.emplace(std::move(lineage).value());
    }
    auto frames = MediaPreparedFrameWrapperStoragePlanner::plan(facts.topology, *result.lineage,
        *facts.videoPlan.decoderReadiness, facts.videoPlan.filterReadiness);
    if (!frames) return R::failure(frames.error());
    const std::array<std::size_t, 2> datagrams{session.video.workingSet.maximumDatagramBytes,
        audio ? session.audio->workingSet.maximumDatagramBytes : 0};
    auto queued = add(facts.video.containers.outputObjects,
        audio ? facts.audio->containers.outputObjects : 0);
    if (!queued) return R::failure(queued.error());
    auto sourceClock = MediaRtcpPreparationStoragePlanner::plan(
        std::span<const std::size_t>{datagrams.data(), audio ? 2u : 1u},
        queued.value(), MediaRtcpPreparationUse::SourceClock);
    if (!sourceClock) return R::failure(sourceClock.error());
    auto startup = MediaCompositionStartupStoragePlanner::plan(facts.startupRoute,
        facts.startupVideoUnits, facts.startupAudioUnits, facts.groupKeyCharacters,
        std::span<const std::size_t>{datagrams.data(), audio ? 2u : 1u});
    if (!startup) return R::failure(startup.error());
    auto events = add(queued.value(), startup.value().clock.maximumEvidenceObjects);
    if (!events) return R::failure(events.error());
    // One source-session callback constructs an event under the shared source
    // mutex while old events may remain in outputs or the downstream route.
    // Group moves its selected event out of an existing slot, not into a third
    // retaining slot. No original ingress wrapper crosses snapshot fanout.
    events = add(events.value(), 1);
    if (!events) return R::failure(events.error());
    auto eventGeometry = probeMediaSharedAllocationGeometry<MediaRtpIngressEventBuffer>(
        std::type_identity<std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>>{});
    if (!eventGeometry) return R::failure(eventGeometry.error());
    auto eventBytes = copies(eventGeometry.value(), events.value());
    if (!eventBytes) return R::failure(eventBytes.error());
    if (!facts.videoPlan.decoderOwnershipStorage ||
        facts.videoPlan.filterActive != facts.videoPlan.filterOwnershipStorage.has_value())
        return R::failure(::media::ErrorInfo::invalidArgument(
            "Candidate frame ownership requires prepared producer geometry"));
    auto ownership = MediaSourceFrameOwnershipStoragePlanner::plan(frames.value(),
        facts.producerHolders, *facts.videoPlan.decoderOwnershipStorage,
        facts.videoPlan.filterOwnershipStorage);
    if (!ownership) return R::failure(ownership.error());
    result.fixedBytes = fixed.value();
    result.startupIndexBytes = indexBytes;
    result.startupGenerationBytes = generation.value().allocationBytes;
    result.frameWrappers = std::move(frames).value();
    result.startup = std::move(startup).value();
    result.frameOwnership = std::move(ownership).value();
    result.sourceClock = std::move(sourceClock).value();
    result.ingressEvents = {events.value(), eventBytes.value()};
    result.protocolBytes = result.startupBytes = result.totalBytes = 0;
    const auto accumulate = [](std::size_t& target, std::initializer_list<std::uint64_t> components) -> ::media::Status {
        for (const auto bytes : components) {
            auto total = checked(MediaCheckedArithmetic::add(target, bytes, "Candidate preparation component total"));
            if (!total) return ::media::Status::failure(total.error());
            target = total.value();
        }
        return ::media::Status::success();
    };
    for (const auto* stream : {&result.video, result.audio ? &*result.audio : nullptr}) {
        if (!stream) continue;
        // containerBytes excludes the replaced pending-only lease control fee.
        // Packet wrappers, payload reservation and lease controls are distinct.
        if (auto status = accumulate(result.protocolBytes, {stream->containerBytes,
                stream->leaseControlsBytes, stream->packetWrappers->preparationBytes,
                stream->payload.bytes}); !status) return R::failure(status.error());
    }
    if (auto status = accumulate(result.protocolBytes, {result.sourceClock.cnamePeakBytes,
            result.startup.clock.additionalCnameBytes, result.ingressEvents.bytes}); !status)
        return R::failure(status.error());
    // Route fees are already expanded for concurrent holders. Do not add batch
    // costs again, or ordinary inner strings/audio wrappers to Prepared charges.
    if (auto status = accumulate(result.startupBytes, {result.startupIndexBytes,
            result.startupGenerationBytes, result.frameWrappers.preparationBytes,
            result.lineage->preparationBytes, result.frameOwnership.bytes,
            result.startup.route.preparationBytes, result.startup.coordinator.preparationBytes}); !status)
        return R::failure(status.error());
    if (auto status = accumulate(result.totalBytes,
            {result.fixedBytes, result.protocolBytes, result.startupBytes}); !status)
        return R::failure(status.error());
    return R::success();
}
} // namespace
::media::Result<MediaPreparedSourceCandidateCosts> MediaPreparedSourceCandidateCostPlanner::plan(
    const MediaPreparedRtpSourceSessionPlan& session, const MediaPreparedRtpStorageSnapshot& held,
    const MediaPreparedSourceCandidateCostFacts& facts)
try {
    using R = ::media::Result<MediaPreparedSourceCandidateCosts>;
    MediaPreparedSourceCandidateCosts result{};
    if (auto status = computeCosts<false>(session, held, facts, result); !status) return R::failure(status.error());
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedSourceCandidateCosts>::failure(
        ::media::ErrorInfo::allocationFailed("source candidate cost catalogs"));
}
::media::Status MediaPreparedSourceCandidateCostPlanner::rebind(
    const MediaPreparedRtpSourceSessionPlan& session, const MediaPreparedRtpStorageSnapshot& held,
    const MediaPreparedSourceCandidateCostFacts& facts, MediaPreparedSourceCandidateCosts& costs)
{
    return computeCosts<true>(session, held, facts, costs);
}
}
