#pragma once

#include "internal/graph/sync/startup/MediaAvStartupReadinessIndex.h"

namespace media::ffmpeg::graph {

struct MediaAvStartupWindow final {
    const MediaAvStartupAccessUnit* video;
    const MediaAvStartupAccessUnit* audio;
    MediaRunningTime sourceStart;
};

class MediaAvStartupWindowSelector final {
public:
    static ::media::Result<std::optional<MediaAvStartupWindow>> select(
        const MediaAvStartupReadinessIndex& video,
        const MediaAvStartupReadinessIndex* audio,
        const MediaAvStartupConfig& config,
        MediaAvStartupSelectionWork& work);
};

} // namespace media::ffmpeg::graph
