#pragma once

#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class MediaStartupReleaseTransactionBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(MediaBufferRef release);

    MediaBufferType type() const noexcept override;
    const MediaBufferRef& release() const noexcept;
    const MediaAvSyncGroupKey& groupKey() const noexcept;
    MediaAvStartupReleaseKind releaseKind() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;

private:
    explicit MediaStartupReleaseTransactionBuffer(MediaBufferRef release);
    const MediaAvStartupReleaseBuffer& typedRelease() const noexcept;

    const MediaBufferRef m_release;
};

} // namespace media::ffmpeg::graph
