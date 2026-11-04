#pragma once

#include "internal/graph/sync/lineage/MediaVideoLineageState.h"

namespace media::ffmpeg::graph {

// For owner-thread nodes that retain references but allocate no codec registry.
// The owner must clear its references before acknowledging its purge mailbox.
class MediaVideoReferenceLineageState final : public MediaVideoLineageState {
public:
    MediaVideoReferenceLineageState() : MediaVideoLineageState(true, nullptr) {}
private:
    void clearOwnedLineage(const MediaAvGenerationPurge&) noexcept override {}
};

} // namespace media::ffmpeg::graph
