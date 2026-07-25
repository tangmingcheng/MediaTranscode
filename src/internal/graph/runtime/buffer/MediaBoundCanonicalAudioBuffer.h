#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaCanonicalAudioSamplesBuffer;

class MediaBoundCanonicalAudioBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        MediaAudioPlaybackOrigin audioOrigin);
    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::shared_ptr<MediaCanonicalAudioSamplesBuffer>& media() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;

private:
    MediaBoundCanonicalAudioBuffer(
        std::shared_ptr<MediaCanonicalAudioSamplesBuffer> media,
        MediaAudioPlaybackOrigin audioOrigin);
    std::shared_ptr<MediaCanonicalAudioSamplesBuffer> m_media;
    MediaAudioPlaybackOrigin m_audioOrigin;
};

} // namespace media::ffmpeg::graph
