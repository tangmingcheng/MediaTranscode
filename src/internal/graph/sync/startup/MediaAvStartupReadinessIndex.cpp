#include "internal/graph/sync/startup/MediaAvStartupReadinessIndex.h"

namespace media::ffmpeg::graph {

MediaAvStartupReadinessIndex::MediaAvStartupReadinessIndex(
    MediaAvStartupPresentationSnapshot snapshot)
    : m_units(snapshot)
{
}

MediaAvStartupReadinessIndex MediaAvStartupReadinessIndex::build(
    MediaAvStartupPresentationSnapshot snapshot,
    MediaAvStartupSelectionWork& work)
{
    work.indexedUnits += snapshot.size();
    return MediaAvStartupReadinessIndex{snapshot};
}

const MediaAvStartupPresentationSnapshot&
MediaAvStartupReadinessIndex::units() const noexcept
{
    return m_units;
}

} // namespace media::ffmpeg::graph
