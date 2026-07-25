#pragma once

#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAudioLineageKey final {
    std::uint64_t generation = 0;
    std::string_view sourceIdentity;
    MediaSourceAccessUnitSequence sourceSequence {0};

    friend bool operator==(const MediaAudioLineageKey&,
                           const MediaAudioLineageKey&) noexcept = default;
};

class MediaAudioLineageCapacity final {
public:
    explicit MediaAudioLineageCapacity(std::size_t capacity) noexcept;

    ::media::Status observe(
        const std::shared_ptr<const MediaCanonicalLineage>& lineage);
    ::media::Status observe(
        const std::vector<MediaAudioIntervalFragment>& fragments);
    std::size_t leaseCount() const noexcept;

private:
    std::size_t m_capacity = 0;
    std::vector<MediaAudioLineageKey> m_keys;
};

} // namespace media::ffmpeg::graph
