#include "internal/graph/planner/realtime/MediaRtpClockRouteStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRtcpPreparationStoragePlanner.h"
#include "internal/graph/model/MediaRtpClockRoute.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaRtpClockRouteStoragePlan> MediaRtpClockRouteStoragePlanner::plan(
    MediaTranscodeStreamSet members, MediaRtpControlDelivery delivery, const MediaEdgePolicy& metadata,
    std::span<const std::size_t> datagrams)
{
    using R = ::media::Result<MediaRtpClockRouteStoragePlan>;
    const bool audio = members == MediaTranscodeStreamSet::AudioVideo;
    const std::size_t streams = audio ? 2 : 1;
    if ((!audio && members != MediaTranscodeStreamSet::VideoOnly) || datagrams.size() != streams ||
        !metadata.queuePolicy.bounded || !metadata.queuePolicy.capacity)
        return R::failure(::media::ErrorInfo::invalidArgument("CNAME route requires exact members and bounded metadata"));
    if (delivery != MediaRtpControlDelivery::RawProtocolOwner &&
        delivery != MediaRtpControlDelivery::SharedPreparedSession)
        return R::failure(::media::ErrorInfo::invalidArgument("CNAME route requires declared control delivery"));
    auto wire = MediaRtcpPreparationStoragePlanner::plan(datagrams, 0, MediaRtcpPreparationUse::ParseOnly);
    if (!wire) return R::failure(wire.error());
    auto allocation = wire.value().maximumCnameBytes
        ? MediaPreparationMemoryResource::allocationFootprint(wire.value().maximumCnameBytes, alignof(std::uint8_t))
        : ::media::Result<std::size_t>::success(0);
    if (!allocation) return R::failure(allocation.error());
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto r = MediaCheckedArithmetic::add(a, b, "CNAME route owners");
        if (!r) { failure = r.error(); return std::uint64_t{0}; }
        return r.value();
    };
    const auto mul = [&](std::uint64_t a, std::uint64_t b) {
        auto r = MediaCheckedArithmetic::multiply(a, b, "CNAME route allocation");
        if (!r) { failure = r.error(); return std::uint64_t{0}; }
        return r.value();
    };
    std::uint64_t evidenceSlots = 0, snapshotSlots = 0;
    for (const auto& edge : MediaRtpClockRouteConnections) {
        if (edge.audioOnly && !audio) continue;
        auto& slots = edge.payload == MediaRtpClockRoutePayload::Evidence ? evidenceSlots : snapshotSlots;
        slots = add(slots, metadata.queuePolicy.capacity);
    }
    // Fanout shares one buffer. Summed queue capacities bound distinct live
    // snapshots conservatively; they are not a claim of one copy per edge.
    const auto snapshotCnames = streams + 1;
    const auto evidenceQueue = mul(evidenceSlots, allocation.value());
    // Preserve the admitted raw-node holder and two Group StreamPending inputs.
    // Shared delivery adds its independent ControlOutput pending transfer; the
    // raw payload holder is not reclaimed to offset this new control owner.
    const auto evidenceInFlightObjects = streams *
        (3 + (delivery == MediaRtpControlDelivery::SharedPreparedSession ? 1 : 0));
    const auto evidenceObjects = add(evidenceSlots, evidenceInFlightObjects);
    const auto evidenceInFlight = mul(evidenceInFlightObjects, allocation.value());
    const auto snapshotQueue = mul(mul(snapshotSlots, snapshotCnames), allocation.value());
    // Group publication, fanout current/pending (same buffer), each binder's
    // popped input and the state adapter's popped input. Scalar output has none.
    const auto snapshotInFlight = mul(mul(streams + 3, snapshotCnames), allocation.value());
    // Each binder copies one CNAME field at a time using replacement-before-move.
    // Old locked snapshots belong to the existing source storage product.
    const auto replacements = mul(streams, allocation.value());
    const auto total = add(add(evidenceQueue, evidenceInFlight), add(add(snapshotQueue, snapshotInFlight), replacements));
    if (failure || total > (std::numeric_limits<std::size_t>::max)() ||
        evidenceObjects > (std::numeric_limits<std::size_t>::max)())
        return R::failure(failure ? failure : ::media::ErrorInfo::allocationFailed("CNAME route exceeds addressable storage"));
    return R::success({wire.value().maximumCnameBytes, static_cast<std::size_t>(evidenceQueue),
        static_cast<std::size_t>(evidenceInFlight), static_cast<std::size_t>(snapshotQueue),
        static_cast<std::size_t>(snapshotInFlight), static_cast<std::size_t>(replacements), static_cast<std::size_t>(total),
        static_cast<std::size_t>(evidenceObjects)});
}
}
