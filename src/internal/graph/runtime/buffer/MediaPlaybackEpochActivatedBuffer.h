#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaPlaybackEpochActivatedBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;
    std::optional<std::uint64_t> completedTransitionSequence() const noexcept;

private:
    MediaPlaybackEpochActivatedBuffer(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin,
        std::optional<std::uint64_t> completedTransitionSequence);

    const MediaAvSyncGroupKey m_groupKey;
    const MediaPlaybackEpoch m_epoch;
    const MediaAudioPlaybackOrigin m_audioOrigin;
    const std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph
