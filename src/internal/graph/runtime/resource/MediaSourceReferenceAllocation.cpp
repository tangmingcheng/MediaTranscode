#include "internal/graph/runtime/resource/MediaSourceReferenceAllocation.h"

namespace media::ffmpeg::graph {

::media::Status MediaSourceReferenceAllocation::bindSourceReferenceResource(
    std::shared_ptr<MediaPreparationMemoryResource> allocation)
{
    if (!allocation || m_resource || m_frozen)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source frame processor requires one allocation owner bound before start"));
    m_resource = std::move(allocation);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
