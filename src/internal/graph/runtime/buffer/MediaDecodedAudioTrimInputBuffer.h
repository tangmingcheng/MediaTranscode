#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaCanonicalAudioSamplesBuffer;

class MediaDecodedAudioTrimInputBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaBufferRef media,
        MediaAudioPlaybackOrigin audioOrigin,
        std::uint32_t trimLeadingSamples);
    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::shared_ptr<MediaCanonicalAudioSamplesBuffer>& media() const noexcept;
    const MediaAudioPlaybackOrigin& audioOrigin() const noexcept;
    std::uint32_t trimLeadingSamples() const noexcept;

private:
    MediaDecodedAudioTrimInputBuffer(
        std::shared_ptr<MediaCanonicalAudioSamplesBuffer> media,
        MediaAudioPlaybackOrigin audioOrigin,
        std::uint32_t trimLeadingSamples);
    std::shared_ptr<MediaCanonicalAudioSamplesBuffer> m_media;
    MediaAudioPlaybackOrigin m_audioOrigin;
    std::uint32_t m_trimLeadingSamples;
};

} // namespace media::ffmpeg::graph
