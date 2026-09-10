#include "application/realtime/MediaRealtimeEncodingGroupRegistry.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t> MediaRealtimeEncodingGroupRegistry::allocate(std::uint64_t& next)
{
    if (next == std::numeric_limits<std::uint64_t>::max())
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported("realtime segment identity space is exhausted"));
    return ::media::Result<std::uint64_t>::success(next++);
}

::media::Result<std::uint64_t> MediaRealtimeEncodingGroupRegistry::allocateGroupId()
{
    return allocate(m_nextGroupId);
}

::media::Result<std::uint64_t> MediaRealtimeEncodingGroupRegistry::allocateSegmentId()
{
    return allocate(m_nextSegmentId);
}

std::vector<MediaRealtimeExistingVideoEncodingGroup>
MediaRealtimeEncodingGroupRegistry::available() const
{
    std::vector<MediaRealtimeExistingVideoEncodingGroup> result;
    for (const auto& [id, group] : m_groups) {
        if (!group.draining && group.graphPublished && group.witness && group.codecParameters &&
            !group.branch->failure() && group.branch->state() == MediaRuntimeBranchState::Running)
            result.push_back({id, group.witness, group.encoded});
    }
    return result;
}

} // namespace media::ffmpeg::graph
