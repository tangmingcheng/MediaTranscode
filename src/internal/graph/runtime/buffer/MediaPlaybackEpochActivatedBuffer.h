#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

namespace media::ffmpeg::graph {

class MediaPlaybackEpochActivatedBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);

    MediaBufferType type() const noexcept override;
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;

private:
    MediaPlaybackEpochActivatedBuffer(
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);

    const MediaAvSyncGroupKey m_groupKey;
    const MediaPlaybackEpoch m_epoch;
    const MediaAudioPlaybackOrigin m_audioOrigin;
};

} // namespace media::ffmpeg::graph
