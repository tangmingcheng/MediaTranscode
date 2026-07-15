#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>

extern "C" {
#include <libavutil/buffer.h>
}

namespace media::ffmpeg::graph {

class MediaFfmpegLineageLeaseControl;

class MediaFfmpegLineageToken final {
public:
    MediaFfmpegLineageToken(MediaFfmpegLineageToken&&) noexcept = default;
    MediaFfmpegLineageToken& operator=(MediaFfmpegLineageToken&&) noexcept = default;
    MediaFfmpegLineageToken(const MediaFfmpegLineageToken&) = delete;
    MediaFfmpegLineageToken& operator=(const MediaFfmpegLineageToken&) = delete;

    std::uint64_t identifier = 0;
    std::uint64_t generation = 0;

private:
    MediaFfmpegLineageToken(
        std::uint64_t identifier,
        std::uint64_t generation,
        std::shared_ptr<MediaFfmpegLineageLeaseControl> lease) noexcept;

    std::shared_ptr<MediaFfmpegLineageLeaseControl> m_lease;

    friend class MediaCodecLineageRegistry;
    friend ::media::Result<AVBufferRef*> makeMediaFfmpegLineageOpaque(
        MediaFfmpegLineageToken token);
    friend ::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
        const AVBufferRef* opaque);
};

::media::Result<AVBufferRef*> makeMediaFfmpegLineageOpaque(
    MediaFfmpegLineageToken token);
::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
    const AVBufferRef* opaque);

} // namespace media::ffmpeg::graph
