#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmissionPlanner.h"
#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {

::media::Result<MediaFinalGraphResourceLedgerEntry>
MediaPreparedRtpStorageAdmissionPlanner::plan(const MediaGraph& graph,
    const MediaPreparedRtpStorageReservation& reservation, std::span<const MediaNodeId> owners,
    std::size_t allocationIndex) try
{
    using Result = ::media::Result<MediaFinalGraphResourceLedgerEntry>;
    if (!reservation.allocationOwner || !reservation.maximumAllocationBytes ||
        reservation.authority.empty() || owners.empty() || owners.size() != reservation.streamCount)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP admission requires its owned source group"));
    const MediaNode* video = nullptr;
    const MediaNode* audio = nullptr;
    for (const auto id : owners) {
        const auto* node = graph.findNode(id);
        if (!node || node->kind != MediaNodeKind::RawRtpInput ||
            std::count(owners.begin(), owners.end(), id) != 1)
            return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP admission has invalid retaining nodes"));
        const auto stream = node->options.value("rtp.stream_kind");
        if (stream == "video" && !video) video = node;
        else if (stream == "audio" && !audio) audio = node;
        else return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP admission has repeated or unknown streams"));
    }
    if (!video) return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP admission has no video owner"));
    return Result::success({"prepared-input:" + video->name + ":" + std::to_string(allocationIndex), {},
        MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
        reservation.maximumAllocationBytes, 0, 0, 0, false, reservation.authority,
        {owners.begin(), owners.end()}, reservation.allocationOwner, {}, {}, MediaFinalGraphReferenceKind::RetainingGroup});
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaFinalGraphResourceLedgerEntry>::failure(
        ::media::ErrorInfo::allocationFailed("Prepared RTP storage admission allocation failed"));
}

} // namespace media::ffmpeg::graph
