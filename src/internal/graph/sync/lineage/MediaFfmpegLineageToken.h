#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

#include <cstdint>
#include <memory>

extern "C" {
#include <libavutil/buffer.h>
}

namespace media::ffmpeg::graph {

struct MediaCodecLineageRegistryState;

class MediaFfmpegLineageToken final {
public:
    MediaFfmpegLineageToken(MediaFfmpegLineageToken&&) noexcept;
    MediaFfmpegLineageToken& operator=(MediaFfmpegLineageToken&&) noexcept;
    ~MediaFfmpegLineageToken();
    MediaFfmpegLineageToken(const MediaFfmpegLineageToken&) = delete;
    MediaFfmpegLineageToken& operator=(const MediaFfmpegLineageToken&) = delete;

    std::uint64_t identifier = 0;
    std::uint64_t generation = 0;

private:
    MediaFfmpegLineageToken(
        std::uint64_t identifier,
        std::uint64_t generation,
        std::weak_ptr<MediaCodecLineageRegistryState> state,
        MediaLineageAllocationScope allocation, bool ownsSlot) noexcept;

    void releaseSlot() noexcept;
    std::weak_ptr<MediaCodecLineageRegistryState> m_state;
    MediaLineageAllocationScope m_allocation;
    // Separate from the public observation fields: mutation cannot retarget release.
    std::uint64_t m_slotIdentifier;

    friend class MediaCodecLineageRegistry;
    friend ::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
        const AVBufferRef* opaque);
    friend ::media::Result<AVBufferRef*> makeMediaFfmpegCodecOpaque(
        MediaFfmpegLineageToken token);
};

::media::Result<AVBufferRef*> makeMediaFfmpegCodecOpaque(
    MediaFfmpegLineageToken token);
::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
    const AVBufferRef* opaque);

} // namespace media::ffmpeg::graph
