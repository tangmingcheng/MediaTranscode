#pragma once

#include "internal/graph/sync/startup/MediaAvStartupCoverageIndex.h"

namespace media::ffmpeg::graph {

struct MediaAvStartupWindow final {
    const MediaAvStartupAccessUnit* video;
    const MediaAvStartupAccessUnit* audio;
    MediaRunningTime sourceStart;
};

class MediaAvStartupWindowSelector final {
public:
    static ::media::Result<std::optional<MediaAvStartupWindow>> select(
        const MediaAvStartupCoverageIndex& video,
        const MediaAvStartupCoverageIndex& audio,
        const MediaAvStartupConfig& config,
        MediaAvStartupSelectionWork& work);
};

} // namespace media::ffmpeg::graph
