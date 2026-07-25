#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

namespace media::ffmpeg::graph {

class MediaCanonicalVideoFrameBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage);
    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept { return m_lineage; }
private:
    MediaCanonicalVideoFrameBuffer(MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage);
    MediaBufferRef m_media;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
};

} // namespace media::ffmpeg::graph
