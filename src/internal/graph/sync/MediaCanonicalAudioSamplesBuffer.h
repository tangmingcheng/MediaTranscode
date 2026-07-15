#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

namespace media::ffmpeg::graph {

class MediaCanonicalAudioSamplesBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        MediaCanonicalAudioSampleInterval interval);
    MediaBufferType type() const noexcept override;
    const MediaBufferRef& media() const noexcept { return m_media; }
    const std::shared_ptr<const MediaCanonicalLineage>& lineage() const noexcept { return m_lineage; }
    const MediaCanonicalAudioSampleInterval& interval() const noexcept { return m_interval; }
private:
    MediaCanonicalAudioSamplesBuffer(MediaBufferRef media,
        std::shared_ptr<const MediaCanonicalLineage> lineage,
        MediaCanonicalAudioSampleInterval interval);
    MediaBufferRef m_media;
    std::shared_ptr<const MediaCanonicalLineage> m_lineage;
    MediaCanonicalAudioSampleInterval m_interval;
};

} // namespace media::ffmpeg::graph
