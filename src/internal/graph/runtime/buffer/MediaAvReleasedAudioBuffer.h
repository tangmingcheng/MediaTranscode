#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaCanonicalAccessUnitBuffer;

class MediaAvReleasedAudioBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        std::uint32_t trimLeadingSamples,
        MediaAudioPlaybackOrigin audioOrigin);

    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::shared_ptr<MediaCanonicalAccessUnitBuffer>& media() const noexcept;
    std::uint32_t trimLeadingSamples() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;

private:
    MediaAvReleasedAudioBuffer(std::shared_ptr<MediaCanonicalAccessUnitBuffer> media,
                               std::uint32_t trimLeadingSamples,
                               MediaAudioPlaybackOrigin audioOrigin);

    std::shared_ptr<MediaCanonicalAccessUnitBuffer> m_media;
    std::uint32_t m_trimLeadingSamples;
    MediaAudioPlaybackOrigin m_audioOrigin;
};

} // namespace media::ffmpeg::graph
