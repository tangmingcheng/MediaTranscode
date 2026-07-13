#pragma once

#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"

namespace media::ffmpeg::graph {

struct MediaAvStartupCoveredUnit final {
    const MediaAvStartupAccessUnit* unit;
    MediaRunningTime coverageEnd;
};

class MediaAvStartupCoverageIndex final {
public:
    static MediaAvStartupCoverageIndex build(
        std::vector<MediaAvStartupIndexedUnit> snapshot,
        MediaAvStartupSelectionWork& work);

    const std::vector<MediaAvStartupCoveredUnit>& units() const noexcept;

private:
    std::vector<MediaAvStartupCoveredUnit> m_units;
};

} // namespace media::ffmpeg::graph
