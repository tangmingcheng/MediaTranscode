#include "internal/graph/planner/realtime/MediaRtpConfigurationStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/nodes/input/MediaRtpControlOutputNode.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"
#include "internal/graph/planner/realtime/MediaRtpProtocolOrdinaryStoragePlanner.h"
#include <algorithm>
#include <limits>
#include <array>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::size_t> resourceOwnerBytes()
{
    using Resource = MediaPreparationMemoryResource;
    auto geometry = probeMediaSharedAllocationGeometry<Resource>(std::type_identity<Resource::BootstrapAllocator>{});
    if (!geometry) return ::media::Result<std::size_t>::failure(geometry.error());
    return Resource::bootstrapAllocationFootprint(geometry.value().bytes, geometry.value().alignment);
}

template<class T>
::media::Result<std::size_t> returnedVectorBytes(std::size_t count)
{
    auto container = MediaPmrContainerGeometry::vector<T>(count, false);
    auto proxy = MediaPmrContainerGeometry::proxyBytes();
    if (!container) return ::media::Result<std::size_t>::failure(container.error());
    if (!proxy) return ::media::Result<std::size_t>::failure(proxy.error());
    // Named local, Result::success argument, Result private constructor argument,
    // and optional member coexist during return. Backing is transferred once.
    auto extra = MediaCheckedArithmetic::multiply(proxy.value(), 3, "Returned protocol vector proxies");
    if (!extra) return ::media::Result<std::size_t>::failure(extra.error());
    auto bytes = MediaCheckedArithmetic::add(container.value().peakBytes, extra.value(), "Returned protocol vector");
    if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::allocationFailed("Returned protocol vector overflow"));
    return ::media::Result<std::size_t>::success(static_cast<std::size_t>(bytes.value()));
}
}

::media::Result<std::size_t> MediaPreparedRtpSourceStoragePlanner::completionOutputObjects(
    const MediaPreparedRtpSourceSessionStreamPlan& stream)
{
    using Result = ::media::Result<std::size_t>;
    if (auto status = stream.envelope.validate(); !status) return Result::failure(status.error());
    if (!stream.reorder.windowPackets ||
        stream.workingSet.maximumAccessUnitsPerPush != stream.envelope.maximumAccessUnitsPerPush)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared completion requires matching reorder and AU bounds"));
    auto packets = MediaCheckedArithmetic::add(stream.reorder.windowPackets, 1,
        "Prepared completion reorder insertion");
    if (!packets) return Result::failure(packets.error());
    if (packets.value() != stream.workingSet.maximumReorderedPackets)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared completion differs from protocol working set"));
    // Pending may contain N packets AND N discontinuities. Each packet can
    // publish A access units plus one evidence event. tickLocked can then
    // publish one clock invalidation. takeOutput uses the same source mutex.
    auto perPacket = MediaCheckedArithmetic::add(stream.envelope.maximumAccessUnitsPerPush, 2,
        "Prepared completion AU, evidence and discontinuity objects");
    if (!perPacket) return Result::failure(perPacket.error());
    auto objects = MediaCheckedArithmetic::multiply(packets.value(), perPacket.value(),
        "Prepared serialized completion objects");
    if (!objects) return Result::failure(objects.error());
    objects = MediaCheckedArithmetic::add(objects.value(), 1, "Prepared completion clock expiry");
    if (!objects) return Result::failure(objects.error());
    if (objects.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Prepared completion exceeds addressable object count"));
    return Result::success(static_cast<std::size_t>(objects.value()));
}

::media::Result<MediaPreparedRtpContainerBounds> MediaPreparedRtpSourceStoragePlanner::minimumBounds(
    const MediaPreparedRtpSourceSessionStreamPlan& stream, const MediaPreparedRtpStreamStorageSnapshot& held)
{
    using Result = ::media::Result<MediaPreparedRtpContainerBounds>;
    auto completion = completionOutputObjects(stream);
    if (!completion) return Result::failure(completion.error());
    if (held.pendingLeases > held.pendingCapacity || held.pendingLeases > stream.envelope.maximumAccessUnitsPerPush)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared snapshot exceeds its pending completion facts"));
    // One slot permits future input when the live snapshot is empty. No arrival
    // rate, source cadence or lifetime horizon is inferred from this floor.
    return Result::success({(std::max)({std::size_t{1}, held.admittedUnits, held.prefixObjects}),
        (std::max)(std::size_t{1}, held.acquiringObjects),
        (std::max)(completion.value(), held.outputObjects)});
}

::media::Result<MediaPreparedRtpContainerStoragePlan> MediaPreparedRtpSourceStoragePlanner::containers(
    const MediaPreparedRtpSourceSessionStreamPlan& stream, MediaPreparedRtpContainerBounds bounds, bool snapshot)
{
    using R = ::media::Result<MediaPreparedRtpContainerStoragePlan>;
    using Lease = MediaPreparationStorageLease;
    const auto prefixCount = bounds.prefixObjects;
    // Zero bounds are meaningful for the observed pre-freeze history. Future
    // admitted live capacities are separately required positive by the caller.
    if (!stream.envelope.maximumAccessUnitsPerPush)
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared container geometry requires finite source bounds"));
    auto outputs = MediaPmrContainerGeometry::deque<MediaPreparedRtpOutput>(bounds.outputObjects, true);
    auto acquiring = MediaPmrContainerGeometry::deque<MediaBufferRef>(bounds.acquiringObjects, true);
    auto prefix = MediaPmrContainerGeometry::deque<MediaBufferRef>(prefixCount, false);
    auto pending = MediaPmrContainerGeometry::vector<std::shared_ptr<Lease>>(stream.envelope.maximumAccessUnitsPerPush, true);
    auto references = MediaPmrContainerGeometry::vector<MediaBufferRef>(snapshot ? prefixCount : 0, false);
    if (!outputs) return R::failure(outputs.error());
    if (!acquiring) return R::failure(acquiring.error());
    if (!prefix) return R::failure(prefix.error());
    if (!pending) return R::failure(pending.error());
    if (!references) return R::failure(references.error());
    auto control = probeMediaSharedAllocationGeometry<Lease>(
        std::type_identity<std::pmr::polymorphic_allocator<Lease>>{});
    if (!control) return R::failure(control.error());
    auto controlRequest = MediaPreparationMemoryResource::allocationFootprint(control.value().bytes, control.value().alignment);
    if (!controlRequest) return R::failure(controlRequest.error());
    auto controls = MediaCheckedArithmetic::multiply(stream.envelope.maximumAccessUnitsPerPush,
        controlRequest.value(), "Prepared pending lease controls");
    if (!controls || controls.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Prepared pending lease controls overflow"));
    // Each FIFO can independently reach the shared object limit over time and
    // retain its map/blocks. Account both real container peaks, not one sum size.
    auto outputStorage = MediaCheckedArithmetic::multiply(outputs.value().peakBytes, 2,
        "Prepared packet and control FIFO storage");
    if (!outputStorage || outputStorage.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Prepared output FIFO storage overflow"));
    MediaPreparedRtpContainerStoragePlan result{prefixCount, stream.envelope.maximumAccessUnitsPerPush,
        static_cast<std::size_t>(outputStorage.value()), acquiring.value().peakBytes, prefix.value().peakBytes,
        pending.value().peakBytes, static_cast<std::size_t>(controls.value()),
        snapshot ? references.value().peakBytes : 0, 0};
    if (snapshot) {
        auto owner = resourceOwnerBytes();
        if (!owner) return R::failure(owner.error());
        auto object = MediaPreparationMemoryResource::allocationFootprint(
            sizeof(std::pmr::vector<MediaBufferRef>), alignof(std::pmr::vector<MediaBufferRef>));
        if (!object) return R::failure(object.error());
        auto sum = MediaCheckedArithmetic::add(result.snapshotBytes, object.value(),
            "Prepared stable snapshot owner");
        if (!sum || sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Prepared snapshot owner overflow"));
        result.snapshotBytes = static_cast<std::size_t>(sum.value());
        auto full = MediaCheckedArithmetic::add(result.snapshotBytes, owner.value(), "Snapshot resource owner");
        if (!full || full.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Snapshot resource owner overflow"));
        result.snapshotBytes = static_cast<std::size_t>(full.value());
    }
    auto pendingCount = MediaCheckedArithmetic::multiply(stream.workingSet.maximumReorderedPackets, 2,
        "Protocol pending capacity");
    if (!pendingCount || pendingCount.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Protocol pending capacity overflow"));
    using Pending = MediaRtpInputProtocolSession::PendingItem;
    // observe/expire reject a nonempty pending queue: each batch starts at
    // offset zero and drains completely before the next enqueue.
    auto protocolPending = MediaPmrContainerGeometry::deque<Pending>(static_cast<std::size_t>(pendingCount.value()), false);
    auto pendingObject = MediaPreparationMemoryResource::allocationFootprint(
        sizeof(std::pmr::deque<Pending>), alignof(std::pmr::deque<Pending>));
    auto reorderObject = MediaPreparationMemoryResource::allocationFootprint(
        sizeof(MediaRtpReorderBuffer), alignof(MediaRtpReorderBuffer));
    if (!protocolPending) return R::failure(protocolPending.error());
    if (!pendingObject) return R::failure(pendingObject.error());
    if (!reorderObject) return R::failure(reorderObject.error());
    auto pendingTotal = MediaCheckedArithmetic::add(protocolPending.value().peakBytes, pendingObject.value(),
        "Protocol pending owner");
    if (!pendingTotal || pendingTotal.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Protocol pending owner overflow"));
    result.protocolPendingPeakBytes = static_cast<std::size_t>(pendingTotal.value());
    auto protocolObjects = MediaCheckedArithmetic::add(result.protocolPendingPeakBytes, reorderObject.value(),
        "Protocol stable reorder owner");
    if (!protocolObjects || protocolObjects.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Protocol stable reorder owner overflow"));
    result.protocolPendingPeakBytes = static_cast<std::size_t>(protocolObjects.value());
    auto requests = returnedVectorBytes<std::uint64_t>(stream.envelope.maximumAccessUnitsPerPush);
    auto completion = returnedVectorBytes<MediaRtpAccessUnit>(stream.envelope.maximumAccessUnitsPerPush);
    if (!requests) return R::failure(requests.error());
    if (!completion) return R::failure(completion.error());
    result.protocolRequestPeakBytes = requests.value();
    result.completionPeakBytes = completion.value();
    if (stream.envelope.codecName == "aac") {
        // Header count is validated before reserve, AU payload validity after.
        // Thus malformed packets are bounded by wire header bits and datagram,
        // not by the later valid-completion bound.
        const auto datagram = stream.workingSet.maximumDatagramBytes;
        const auto headers = static_cast<std::size_t>(
            MediaAacRtpAuHeaderLayout::maximumParsedHeaders(datagram));
        auto bytes = returnedVectorBytes<MediaAacRtpAuHeader>(headers);
        if (!bytes) return R::failure(bytes.error());
        result.aacHeaderPeakBytes = bytes.value();
    }
    // Each owner's peak includes its old/new allocation overlap. Summing these
    // is a safe candidate envelope, not a claim that all peaks occur together.
    for (auto bytes : {result.outputPeakBytes, result.acquiringPeakBytes, result.prefixPeakBytes,
        result.pendingPeakBytes, result.pendingLeaseControlBytes, result.snapshotBytes,
        result.protocolPendingPeakBytes, result.protocolRequestPeakBytes, result.completionPeakBytes,
        result.aacHeaderPeakBytes}) {
        auto sum = MediaCheckedArithmetic::add(result.peakBytes, bytes, "Prepared container candidate envelope");
        if (!sum || sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Prepared container candidate envelope overflow"));
        result.peakBytes = static_cast<std::size_t>(sum.value());
    }
    return R::success(result);
}

::media::Status MediaPreparedRtpSourceStoragePlanner::plan(MediaPreparedRtpSourceSessionPlan& p)
{
    if (!p.admission.sharedCapacityBytes || !p.admission.video.maximumUnits ||
        p.audio.has_value() != p.admission.audio.has_value() ||
        (p.admission.audio && !p.admission.audio->maximumUnits))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP storage requires deployment admission"));
    if (auto audio = p.validateAudioRetention(); !audio) return audio;
    if (p.canonicalRuntimeRetention) {
        if (auto status = p.canonicalRuntimeRetention->validate(); !status) return status;
    }
    const std::array<std::size_t, 2> datagrams{p.video.workingSet.maximumDatagramBytes,
        p.audio ? p.audio->workingSet.maximumDatagramBytes : 0};
    auto queued = MediaCheckedArithmetic::add(p.video.maximumProtocolOutputObjects,
        p.audio ? p.audio->maximumProtocolOutputObjects : 0, "RTCP queued evidence owners");
    if (!queued || queued.value() > (std::numeric_limits<std::size_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("RTCP queued evidence owner overflow"));
    auto rtcp = MediaRtcpPreparationStoragePlanner::plan(
        std::span<const std::size_t>{datagrams.data(), p.audio ? 2u : 1u},
        static_cast<std::size_t>(queued.value()), MediaRtcpPreparationUse::SourceClock);
    if (!rtcp) return ::media::Status::failure(rtcp.error());
    p.rtcpStorage = std::move(rtcp).value();
    p.video.containerStorage.maximumPrefixObjects = p.admission.video.maximumUnits;
    p.video.containerStorage.maximumPendingLeases = p.video.envelope.maximumAccessUnitsPerPush;
    if (p.audio) {
        p.audio->containerStorage.maximumPrefixObjects = p.admission.audio->maximumUnits;
        p.audio->containerStorage.maximumPendingLeases = p.audio->envelope.maximumAccessUnitsPerPush;
    }
    // Existing cadence preparation uses actual resource admission. Supplier
    // geometry is needed only by the new source-presentation candidate solver.
    if (!p.canonicalRuntimeRetention) {
        auto videoContainers = containers(p.video, {p.admission.video.maximumUnits,
            p.video.binder.acquiringCapacity, p.video.maximumProtocolOutputObjects}, true);
        if (!videoContainers) return ::media::Status::failure(videoContainers.error());
        p.video.containerStorage = std::move(videoContainers).value();
        if (p.audio) {
            auto audioContainers = containers(*p.audio, {p.admission.audio->maximumUnits,
                p.audio->binder.acquiringCapacity, p.audio->maximumProtocolOutputObjects}, false);
            if (!audioContainers) return ::media::Status::failure(audioContainers.error());
            p.audio->containerStorage = std::move(audioContainers).value();
        }
    }
    const auto facts = MediaPreparedRtpSourceSession::storageFacts();
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::add(a, b, "prepared RTP source storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const auto mul = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::multiply(a, b, "prepared RTP source storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const std::uint64_t streams = p.audio ? 2 : 1;
    // These remaining ordinary owners are still covered by the fixed lease.
    // Session/control, Stream, protocol, binder and group now allocate through
    // their real resource and must never be charged by this lease a second time.
    std::uint64_t fixed = add(sizeof(MediaRtpClockGroupValidator),
        mul(streams, sizeof(MediaRtpSourceClockMapper)));
    // Shared prepared sessions own one control consumer per stream. Its ordinary
    // node allocation (including the inline endpoint) is covered by this fixed
    // lease before factory construction, not charged as a fictitious PMR block.
    fixed = add(fixed, mul(streams, sizeof(MediaRtpControlOutputNode)));
    p.fixedOwnerAllocationBytes.reset();
    if (!p.canonicalRuntimeRetention) {
        auto source = probeMediaSharedAllocationGeometry<MediaPreparedRtpSourceSession>(
            std::type_identity<std::pmr::polymorphic_allocator<MediaPreparedRtpSourceSession>>{});
        if (!source) return ::media::Status::failure(source.error());
        const auto request = [&](std::size_t bytes, std::size_t alignment) {
            auto cost = MediaPreparationMemoryResource::allocationFootprint(bytes, alignment);
            if (!cost) { failure = cost.error(); return std::uint64_t{0}; }
            return static_cast<std::uint64_t>(cost.value());
        };
        auto owners = request(source.value().bytes, source.value().alignment);
        auto bootstrap = resourceOwnerBytes();
        if (!bootstrap) return ::media::Status::failure(bootstrap.error());
        owners = add(owners, mul(add(streams, 1), bootstrap.value()));
        owners = add(owners, request(sizeof(MediaRtpClockGroupSession), alignof(MediaRtpClockGroupSession)));
        auto perStream = request(facts.streamBytes, facts.streamAlignment);
        perStream = add(perStream, request(sizeof(MediaRtpInputProtocolSession), alignof(MediaRtpInputProtocolSession)));
        perStream = add(perStream, request(sizeof(MediaRtpPacketClockSession), alignof(MediaRtpPacketClockSession)));
        perStream = add(perStream, request(sizeof(MediaPreparedRtpPrefix), alignof(MediaPreparedRtpPrefix)));
        perStream = add(perStream, request(sizeof(std::pmr::deque<MediaBufferRef>), alignof(std::pmr::deque<MediaBufferRef>)));
        owners = add(owners, mul(streams, perStream));
        // SourceSession copies each installed identity once into this resource.
        // Plan/binder/lineage copies then share that immutable array. Its custom
        // deleter control is separate from the exact character backing.
        using IdentityAllocator = MediaLineageAllocator<char>;
        using IdentityDeleter = MediaImmutableArray<char>::AllocationDeleter<IdentityAllocator>;
        auto identityControl = probeMediaAllocatedDeleterControlGeometry<
            char*, IdentityDeleter, IdentityAllocator>();
        if (!identityControl) return ::media::Status::failure(identityControl.error());
        for (const auto* stream : {&p.video, p.audio ? &*p.audio : nullptr}) {
            if (!stream || !stream->binder.videoTiming ||
                !stream->binder.videoTiming->sourceIdentity ||
                stream->binder.videoTiming->sourceIdentity->empty()) continue;
            owners = add(owners, request(stream->binder.videoTiming->sourceIdentity->view().size(), alignof(char)));
            owners = add(owners, request(identityControl.value().bytes, identityControl.value().alignment));
        }
        if (failure || owners > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(failure ? failure :
                ::media::ErrorInfo::allocationFailed("Prepared fixed owner allocation overflow"));
        p.fixedOwnerAllocationBytes = static_cast<std::size_t>(owners);
    }
    auto signaling = parseRtpVideoSignalingFacts(p.video.depacketizer.codecName, p.video.depacketizer.fmtp);
    if (!signaling) return ::media::Status::failure(signaling.error());
    p.videoSignaling.emplace(std::move(signaling).value());
    std::uint64_t parameterBytes = 0;
    if (const auto* h264 = std::get_if<MediaH264SignalingFacts>(&*p.videoSignaling))
        parameterBytes = add(add(h264->sps.size(), h264->pps.size()), h264->profileLevelId.size());
    else {
        const auto& hevc = std::get<MediaHevcSignalingFacts>(*p.videoSignaling);
        parameterBytes = add(add(hevc.vps.size(), hevc.sps.size()), hevc.pps.size());
    }
    if (!p.canonicalRuntimeRetention) {
        const auto bytes = [&](const auto& container) -> std::uint64_t {
            auto cost = [&] {
                if constexpr (std::is_same_v<std::decay_t<decltype(container)>, std::string>)
                    return MediaStandardContainerStorageReadback::string(container);
                else return MediaStandardContainerStorageReadback::vector(container);
            }();
            if (!cost) { failure = cost.error(); return 0; }
            return cost.value();
        };
        parameterBytes = std::visit([&](const auto& value) {
            auto total = add(bytes(value.sps), bytes(value.pps));
            if constexpr (requires { value.vps; }) return add(total, bytes(value.vps));
            else return add(total, bytes(value.profileLevelId));
        }, *p.videoSignaling);
    }
    fixed = add(fixed, parameterBytes);
    std::uint64_t protocolBytes = 0;
    const auto stringBytes = [&](const std::string& value) -> std::uint64_t {
        // Preserve the existing cadence admission domain. Exact readback is a
        // capability requirement of the new source-presentation product only.
        if (p.canonicalRuntimeRetention) return value.size();
        auto bytes = MediaStandardContainerStorageReadback::string(value);
        if (!bytes) { failure = bytes.error(); return 0; }
        return bytes.value();
    };
    const auto account = [&](MediaPreparedRtpSourceSessionStreamPlan& stream) {
        // Stream references m_plan; protocol/depay configuration copies are
        // separately included in protocolWorkingSet.configurationBytes.
        auto strings = add(stringBytes(stream.depacketizer.codecName), stringBytes(stream.depacketizer.fmtp));
        strings = add(strings, stringBytes(stream.envelope.codecName));
        strings = add(strings, stringBytes(stream.envelope.sizeAuthority));
        strings = add(strings, stringBytes(stream.envelope.completionAuthority));
        strings = add(strings, stringBytes(stream.workingSet.authority));
        fixed = add(fixed, strings);
        // Dynamic FIFO blocks/maps, snapshot capacity, shared AU lease owners,
        // packet wrappers and decode-lineage envelopes are charged by their PMR
        // allocation before allocation; do not precharge a deployment-wide count.
        // Source identity characters and control block are charged by the stream resource.
        // Remove only the logical pieces whose real allocations migrated above;
        // raw-datagram probing still consumes the unchanged legacy workspace.
        auto migrated = add(stream.workingSet.pendingResultBytes, stream.workingSet.allocationRequestBytes);
        migrated = add(migrated, mul(stream.envelope.maximumAccessUnitsPerPush, sizeof(MediaRtpAccessUnit)));
        if (stream.envelope.codecName == "aac")
            migrated = add(migrated, mul(stream.workingSet.maximumDatagramBytes / 2, sizeof(MediaAacRtpAuHeader)));
        if (migrated >= stream.workingSet.maximumBytes) {
            failure = ::media::ErrorInfo::invalidArgument("Migrated protocol owners exceed legacy workspace");
            return;
        }
        if (migrated > stream.workingSet.costs.metadataBytes ||
            stream.workingSet.costs.totalBytes != stream.workingSet.maximumBytes) {
            failure = ::media::ErrorInfo::invalidArgument("Migrated protocol metadata exceeds its classified owner");
            return;
        }
        stream.unmigratedProtocolStorage = {
            static_cast<std::size_t>(stream.workingSet.costs.metadataBytes - migrated),
            stream.workingSet.costs.compressedBytes,
            static_cast<std::size_t>(stream.workingSet.maximumBytes - migrated)};
        if (!p.canonicalRuntimeRetention) {
            auto ordinary = MediaRtpProtocolOrdinaryStoragePlanner::plan(stream);
            if (!ordinary) { failure = ordinary.error(); return; }
            auto configuration = MediaRtpConfigurationStoragePlanner::plan(stream.depacketizer, stream.envelope);
            if (!configuration) { failure = configuration.error(); return; }
            const auto reserved = add(ordinary.value().totalBytes, configuration.value().peakBytes);
            if (failure || reserved > (std::numeric_limits<std::size_t>::max)()) {
                if (!failure) failure = ::media::ErrorInfo::allocationFailed("Protocol ordinary reserve overflow");
                return;
            }
            stream.unmigratedProtocolStorage = {
                static_cast<std::size_t>(reserved - ordinary.value().compressedBytes),
                ordinary.value().compressedBytes, static_cast<std::size_t>(reserved)};
        }
        protocolBytes = add(protocolBytes, stream.unmigratedProtocolStorage.totalBytes);
    };
    account(p.video);
    if (p.audio) account(*p.audio);
    if (p.canonicalRuntimeRetention) {
        fixed = add(fixed, p.canonicalRuntimeRetention->video.authority.size());
        if (p.canonicalRuntimeRetention->audio)
            fixed = add(fixed, p.canonicalRuntimeRetention->audio->authority.size());
    }
    if (p.audioRetention) fixed = add(fixed, stringBytes(p.audioRetention->authority));
    const auto minimum = add(add(fixed, protocolBytes), p.fixedOwnerAllocationBytes.value_or(0));
    if (failure) return ::media::Status::failure(failure);
    if (!fixed || minimum > p.admission.sharedCapacityBytes)
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "Prepared RTP fixed and protocol owners exceed deployment capacity"));
    p.fixedStorageBytes = static_cast<std::size_t>(fixed);
    p.storageBytes = p.admission.sharedCapacityBytes;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
