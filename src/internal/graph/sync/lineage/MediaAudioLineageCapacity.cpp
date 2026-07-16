#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

#include <algorithm>

namespace media::ffmpeg::graph {

MediaAudioLineageCapacity::MediaAudioLineageCapacity(
    std::size_t capacity) noexcept
    : m_capacity(capacity)
{
}

::media::Status MediaAudioLineageCapacity::observe(
    const std::shared_ptr<const MediaCanonicalLineage>& lineage)
{
    if (!lineage || !validateMediaCanonicalLineage(*lineage)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio lineage capacity requires valid canonical lineage"));
    }
    MediaAudioLineageKey key{
        lineage->generation, lineage->sourceIdentity, lineage->sourceSequence};
    if (std::find(m_keys.begin(), m_keys.end(), key) != m_keys.end()) {
        return ::media::Status::success();
    }
    if (m_keys.size() >= m_capacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio canonical lineage lease capacity exhausted"));
    }
    m_keys.push_back(key);
    return ::media::Status::success();
}

::media::Status MediaAudioLineageCapacity::observe(
    const std::vector<MediaAudioIntervalFragment>& fragments)
{
    if (fragments.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio lineage capacity requires at least one interval"));
    }
    for (const auto& fragment : fragments) {
        if (auto status = observe(fragment.lineage); !status) {
            return status;
        }
    }
    return ::media::Status::success();
}

std::size_t MediaAudioLineageCapacity::leaseCount() const noexcept
{
    return m_keys.size();
}

} // namespace media::ffmpeg::graph
