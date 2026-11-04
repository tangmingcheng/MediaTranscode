#pragma once

#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"

namespace media::ffmpeg::graph {

// Non-owning view of Store scratch, valid only for the current selection call.
class MediaAvStartupReadinessIndex final {
public:
    static MediaAvStartupReadinessIndex build(
        MediaAvStartupPresentationSnapshot snapshot,
        MediaAvStartupSelectionWork& work);

    const MediaAvStartupPresentationSnapshot& units() const noexcept;

private:
    explicit MediaAvStartupReadinessIndex(MediaAvStartupPresentationSnapshot snapshot);

    MediaAvStartupPresentationSnapshot m_units;
};

} // namespace media::ffmpeg::graph
