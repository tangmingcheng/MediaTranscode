#include "internal/graph/planner/realtime/MediaStartupBatchStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaStartupSharedObjectPlanner.h"
#include "internal/graph/planner/realtime/MediaOutputReservationStoragePlanner.h"
#include "internal/graph/model/MediaStartupReservationOwners.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <array>
#include <limits>
namespace media::ffmpeg::graph {
::media::Result<MediaStartupBatchStorageCosts> MediaStartupBatchStoragePlanner::plan(
    const MediaStartupRoutePlan& route, std::size_t videoUnits,
    std::optional<std::size_t> audioUnits, const MediaOutputReservationLayout& reservation,
    std::size_t groupKeyCharacters)
{
    using R = ::media::Result<MediaStartupBatchStorageCosts>;
    auto media = MediaStartupRouteMediaPlanner::plan(route, videoUnits, audioUnits);
    if (!media) return R::failure(media.error());
    if (!groupKeyCharacters) return R::failure(::media::ErrorInfo::invalidArgument("Startup batch requires its source group"));
    auto group = MediaStandardContainerStorageReadback::stringCopy(groupKeyCharacters);
    if (!group) return R::failure(group.error());
    std::size_t audioWrappers = 0;
    if (audioUnits) {
#if !defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION != 145 || _MSVC_STL_UPDATE != 202604L
        return R::failure(::media::ErrorInfo::unsupported("Released audio wrapper requires audited shared pointer geometry"));
#else
        // Actual create uses shared_ptr<MediaBuffer>(new Derived), so the
        // ordinary object and _Ref_count<Derived> are separate allocations.
        auto bytes = MediaCheckedArithmetic::multiply(*audioUnits,
            sizeof(MediaAvReleasedAudioBuffer) + sizeof(std::_Ref_count<MediaAvReleasedAudioBuffer>),
            "Released audio wrapper allocations");
        if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Released audio wrappers overflow"));
        audioWrappers = static_cast<std::size_t>(bytes.value());
#endif
    }
    auto envelope = MediaStartupSharedObjectPlanner::prepared(MediaStartupSharedObjectRole::Envelope);
    auto release = MediaStartupSharedObjectPlanner::prepared(MediaStartupSharedObjectRole::Release);
    auto transaction = MediaStartupSharedObjectPlanner::prepared(MediaStartupSharedObjectRole::ReleaseTransaction);
    if (!envelope || !release || !transaction)
        return R::failure(!envelope ? envelope.error() : !release ? release.error() : transaction.error());
    std::size_t arrays = 0;
    std::size_t staging = 0;
    for (auto count : {videoUnits, audioUnits.value_or(0)}) {
        if (!count) continue;
        auto bytes = MediaCheckedArithmetic::multiply(count, sizeof(MediaAvReleasedUnit), "Startup released array");
        if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup released array overflow"));
        auto cost = MediaPreparationMemoryResource::allocationFootprint(static_cast<std::size_t>(bytes.value()), alignof(MediaAvReleasedUnit));
        if (!cost) return R::failure(cost.error());
        auto sum = MediaCheckedArithmetic::add(arrays, cost.value(), "Startup released arrays");
        if (!sum || sum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup released arrays overflow"));
        arrays = static_cast<std::size_t>(sum.value());
        auto referenceBytes = MediaCheckedArithmetic::multiply(count, sizeof(MediaBufferRef), "Startup staging array");
        if (!referenceBytes || referenceBytes.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup staging array overflow"));
        auto staged = MediaPreparationMemoryResource::allocationFootprint(static_cast<std::size_t>(referenceBytes.value()), alignof(MediaBufferRef));
        if (!staged) return R::failure(staged.error());
        auto stagedSum = MediaCheckedArithmetic::add(staging, staged.value(), "Startup staging arrays");
        if (!stagedSum || stagedSum.value() > (std::numeric_limits<std::size_t>::max)())
            return R::failure(::media::ErrorInfo::allocationFailed("Startup staging arrays overflow"));
        staging = static_cast<std::size_t>(stagedSum.value());
    }
    const std::array<std::size_t, 1> preparedCounts{1};
    const std::array<std::size_t, 2> extractedCounts{videoUnits, audioUnits.value_or(0)};
    const std::size_t members = audioUnits ? 2 : 1;
    auto preparation = MediaOutputReservationStoragePlanner::reservationBytes(reservation, preparedCounts, 1, MediaVideoPreparationReservationOwner.size());
    auto extraction = MediaOutputReservationStoragePlanner::reservationBytes(reservation, std::span(extractedCounts).first(members), members, MediaExtractorReservationOwner.size());
    // Two real handles: prepared frame output, plus extractor video/audio.
    // These output ports belong to different nodes and are distinct channels.
    auto authorization = MediaOutputReservationStoragePlanner::authorizationBytes(reservation, 2, members + 1, members + 1);
    if (!preparation || !extraction || !authorization)
        return R::failure(!preparation ? preparation.error() : !extraction ? extraction.error() : authorization.error());
    return R::success({envelope.value().allocationBytes, release.value().allocationBytes,
        transaction.value().allocationBytes, arrays, staging, group.value(), audioWrappers,
        preparation.value(), extraction.value(), authorization.value()});
}
}
