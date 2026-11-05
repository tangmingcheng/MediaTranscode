#include "internal/graph/planner/realtime/MediaPreparedSourceCandidateCostPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <limits>

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
::media::Result<MediaPreparedStreamCandidateCosts> streamCosts(
    const MediaPreparedRtpSourceSessionStreamPlan& stream,
    const MediaPreparedRtpStreamStorageSnapshot& held,
    const MediaPreparedStreamCandidateCostFacts& facts, bool snapshot,
    const MediaPreparedSourceCandidateCostFacts& source, MediaStreamKind streamKind)
{
    using R = ::media::Result<MediaPreparedStreamCandidateCosts>;
    if (held.pendingLeases > held.pendingCapacity ||
        stream.envelope.maximumAccessUnitsPerPush < held.pendingLeases ||
        facts.containers.prefixObjects < held.prefixObjects ||
        facts.containers.acquiringObjects < held.acquiringObjects ||
        facts.containers.outputObjects < held.outputObjects)
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate cannot discard live preparation owners"));
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
    auto wrappers = MediaPreparedPacketWrapperStoragePlanner::plan(source.topology, source.residence,
        source.sourceIndex, streamKind, facts.containers, stream.envelope.maximumAccessUnitsPerPush);
    if (!leases) return R::failure(leases.error());
    if (!wrappers) return R::failure(wrappers.error());
    return R::success({result, result.peakBytes - result.pendingLeaseControlBytes,
        leases.value(), std::move(wrappers).value(), roots});
}
}

::media::Result<MediaPreparedSourceCandidateCosts> MediaPreparedSourceCandidateCostPlanner::plan(
    const MediaPreparedRtpSourceSessionPlan& session, const MediaPreparedRtpStorageSnapshot& held,
    const MediaPreparedSourceCandidateCostFacts& facts)
{
    using R = ::media::Result<MediaPreparedSourceCandidateCosts>;
    const bool audio = session.audio.has_value();
    if (session.canonicalRuntimeRetention || !session.fixedOwnerAllocationBytes || !held.owner ||
        audio != held.audio.has_value() || audio != facts.audio.has_value() ||
        audio != facts.startupAudioUnits.has_value() || !facts.startupVideoUnits ||
        facts.startupVideoUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        (audio && (!*facts.startupAudioUnits || *facts.startupAudioUnits > static_cast<std::size_t>((std::numeric_limits<int>::max)()))))
        return R::failure(::media::ErrorInfo::invalidArgument("Candidate costs require prepared source member facts"));
    auto fixed = add(session.fixedStorageBytes, *session.fixedOwnerAllocationBytes);
    if (!fixed) return R::failure(fixed.error());
    fixed = add(fixed.value(), session.video.unmigratedProtocolStorageBytes);
    if (!fixed) return R::failure(fixed.error());
    if (audio) { fixed = add(fixed.value(), session.audio->unmigratedProtocolStorageBytes); if (!fixed) return R::failure(fixed.error()); }
    auto video = streamCosts(session.video, held.video, facts.video, true, facts, MediaStreamKind::Video);
    if (!video) return R::failure(video.error());
    std::optional<MediaPreparedStreamCandidateCosts> audioCosts;
    if (audio) {
        auto value = streamCosts(*session.audio, *held.audio, *facts.audio, false, facts, MediaStreamKind::Audio);
        if (!value) return R::failure(value.error());
        audioCosts = std::move(value).value();
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
    auto lineage = MediaSourceLineageStoragePlanner::plan(facts.topology, facts.residence,
        facts.sourceIndex, facts.registryCapacity, facts.video.containers.prefixObjects, facts.videoPlan.transferDirection);
    if (!lineage) return R::failure(lineage.error());
    auto frames = MediaPreparedFrameWrapperStoragePlanner::plan(facts.topology, lineage.value(),
        *facts.videoPlan.decoderReadiness, facts.videoPlan.filterReadiness);
    if (!frames) return R::failure(frames.error());
    const std::array<std::size_t, 2> datagrams{session.video.workingSet.maximumDatagramBytes,
        audio ? session.audio->workingSet.maximumDatagramBytes : 0};
    auto startup = MediaCompositionStartupStoragePlanner::plan(facts.startupRoute,
        facts.startupVideoUnits, facts.startupAudioUnits, facts.groupKeyCharacters,
        std::span<const std::size_t>{datagrams.data(), audio ? 2u : 1u});
    if (!startup) return R::failure(startup.error());
    if (!facts.videoPlan.decoderOwnershipStorage ||
        facts.videoPlan.filterActive != facts.videoPlan.filterOwnershipStorage.has_value())
        return R::failure(::media::ErrorInfo::invalidArgument(
            "Candidate frame ownership requires prepared producer geometry"));
    auto ownership = MediaSourceFrameOwnershipStoragePlanner::plan(facts.topology, facts.residence,
        facts.sourceIndex, facts.producerHolders, *facts.videoPlan.decoderOwnershipStorage,
        facts.videoPlan.filterOwnershipStorage);
    if (!ownership) return R::failure(ownership.error());
    return R::success({fixed.value(), std::move(video).value(), std::move(audioCosts),
        indexBytes, generation.value().allocationBytes, std::move(frames).value(), std::move(lineage).value(),
        std::move(startup).value(), std::move(ownership).value()});
}
}
