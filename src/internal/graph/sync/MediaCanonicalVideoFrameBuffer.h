#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/MediaVideoPresentationProgress.h"

namespace media::ffmpeg::graph {

class MediaCanonicalVideoFrameBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaVideoPresentationProgress> progress);
    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept { return m_lineage; }
    const std::optional<MediaVideoPresentationProgress>& presentationProgress() const noexcept
    { return m_progress; }
private:
    MediaCanonicalVideoFrameBuffer(MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        std::optional<MediaVideoPresentationProgress> progress);
    MediaBufferRef m_media;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
    const std::optional<MediaVideoPresentationProgress> m_progress;
};

} // namespace media::ffmpeg::graph
