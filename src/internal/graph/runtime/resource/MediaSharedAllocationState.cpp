#include "internal/graph/runtime/resource/MediaSharedAllocationState.h"

namespace media::ffmpeg::graph {
::media::Result<std::shared_ptr<MediaSharedAllocationState>> MediaSharedAllocationState::create()
{
    using Result = ::media::Result<std::shared_ptr<MediaSharedAllocationState>>;
    try {
        return Result::success(std::shared_ptr<MediaSharedAllocationState>(
            new MediaSharedAllocationState()));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Shared allocation probe state"));
    }
}
void MediaSharedAllocationState::observe(const MediaSharedAllocationGeometry& geometry)
{
    if (m_attempted.exchange(true)) throw MediaSharedAllocationMismatch{};
    m_observed = geometry;
    throw MediaSharedAllocationProbeComplete(this);
}
} // namespace media::ffmpeg::graph
