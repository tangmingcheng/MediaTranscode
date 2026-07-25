#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaEncodedAudioCanonicalizerState final : public MediaAudioLineageState {
public:
    struct PendingOutput final {
        std::shared_ptr<MediaCanonicalAccessUnitBuffer> output;
        std::int64_t nextSample;
        std::uint64_t generation;
        std::uint64_t nextSequence;
    };

    MediaEncodedAudioCanonicalizerState() noexcept;
    void resetForLifecycle() noexcept;

    std::optional<std::int64_t> expectedNextSample;
    std::optional<PendingOutput> pending;
    std::uint64_t nextSequence = 1;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearOwnedState() noexcept;
};

} // namespace media::ffmpeg::graph
