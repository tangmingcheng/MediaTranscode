#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaPlaybackActivation.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaPlaybackEpochActivatedBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackActivation activation,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaPlaybackActivation& activation() const noexcept;
    std::optional<std::uint64_t> completedTransitionSequence() const noexcept;

private:
    MediaPlaybackEpochActivatedBuffer(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackActivation activation,
        std::optional<std::uint64_t> completedTransitionSequence);

    const MediaAvSyncGroupKey m_groupKey;
    MediaPlaybackActivation m_activation;
    const std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph
