#pragma once

#include "internal/graph/runtime/storage/MediaImmutableArray.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaEncodedAudioLineageBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::vector<MediaAudioIntervalFragment> fragments,
        MediaAudioPlaybackOrigin origin);
    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const MediaBufferRef& media() const noexcept;
    std::span<const MediaAudioIntervalFragment> fragments() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;

private:
    MediaEncodedAudioLineageBuffer(
        MediaBufferRef media,
        MediaImmutableArray<MediaAudioIntervalFragment> fragments,
        MediaAudioPlaybackOrigin origin);
    MediaBufferRef m_media;
    MediaImmutableArray<MediaAudioIntervalFragment> m_fragments;
    MediaAudioPlaybackOrigin m_origin;
};

} // namespace media::ffmpeg::graph
