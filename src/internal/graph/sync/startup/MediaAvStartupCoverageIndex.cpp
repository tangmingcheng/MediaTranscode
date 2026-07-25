#include "internal/graph/sync/startup/MediaAvStartupCoverageIndex.h"

namespace media::ffmpeg::graph {

MediaAvStartupCoverageIndex MediaAvStartupCoverageIndex::build(
    std::vector<MediaAvStartupIndexedUnit> snapshot,
    MediaAvStartupSelectionWork& work)
{
    MediaAvStartupCoverageIndex index;
    index.m_units.reserve(snapshot.size());
    work.indexedUnits += snapshot.size();
    for (const auto& current : snapshot) {
        index.m_units.push_back({current.unit, current.coverageEnd});
    }
    return index;
}

const std::vector<MediaAvStartupCoveredUnit>&
MediaAvStartupCoverageIndex::units() const noexcept
{
    return m_units;
}

} // namespace media::ffmpeg::graph
