#pragma once

#include "internal/graph/sync/MediaAudioDriftServo.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaAudioDriftControllerState final : public MediaAudioLineageState {
public:
    struct PendingTransaction final {
        MediaBufferRef audio;
        MediaBufferRef correction;
        MediaAudioDriftServo servo;
        MediaAudioSampleProjection projection;
        MediaAudioPlaybackOrigin origin;
        std::uint64_t nextSequence;
        MediaAvOutputPermitCommitReservation outputPermit;
    };

    MediaAudioDriftControllerState() noexcept;
    void resetForLifecycle() noexcept;

    std::optional<MediaAudioDriftServo> servo;
    std::optional<MediaAudioSampleProjection> projection;
    std::optional<MediaAudioPlaybackOrigin> origin;
    std::optional<PendingTransaction> pending;
    std::uint64_t nextSequence = 1;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearOwnedState() noexcept;
};

} // namespace media::ffmpeg::graph
