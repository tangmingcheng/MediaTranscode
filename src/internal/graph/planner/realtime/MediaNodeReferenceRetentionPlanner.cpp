#include "internal/graph/planner/realtime/MediaNodeReferenceRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
namespace media::ffmpeg::graph {
::media::Result<std::uint64_t> MediaNodeReferenceRetentionPlanner::plan(const MediaNodeReferenceRetentionFacts& facts)
{
    using R = ::media::Result<std::uint64_t>;
    if ((facts.kind == MediaNodeKind::RtpPacketClockBinder) != facts.rtpAcquiringCapacity.has_value() ||
        (facts.kind == MediaNodeKind::RawRtpInput) != facts.rawDescriptorCapacity.has_value() ||
        (facts.kind == MediaNodeKind::VideoFilter) != facts.filterRetainedInputs.has_value())
        return R::failure(::media::ErrorInfo::invalidArgument("Node reference retention requires matching typed holder facts"));
    // Existing conservative node contract: one working reference plus every
    // incident queue's capacity, independent of queue's SPSC sentinel storage.
    std::uint64_t retained = 1;
    const auto add = [&](std::uint64_t count, const char* label) -> ::media::Status {
        auto sum = MediaCheckedArithmetic::add(retained, count, label);
        if (!sum) return ::media::Status::failure(sum.error());
        retained = sum.value();
        return ::media::Status::success();
    };
    for (const auto capacity : facts.incidentCapacities)
        if (auto status = add(capacity, "node port retention bound"); !status) return R::failure(status.error());
    if (facts.rtpAcquiringCapacity)
        if (auto status = add(*facts.rtpAcquiringCapacity, "RTP clock acquisition object credits"); !status) return R::failure(status.error());
    if (facts.filterRetainedInputs) {
        constexpr std::uint64_t codecAndPendingReferences = 2;
        if (auto status = add(codecAndPendingReferences, "video filter codec and pending references"); !status) return R::failure(status.error());
        if (auto status = add(*facts.filterRetainedInputs, "source filter adapter retained input frames"); !status) return R::failure(status.error());
    }
    if (facts.rawDescriptorCapacity) retained = (std::max)(retained, *facts.rawDescriptorCapacity);
    return R::success(retained);
}
}
