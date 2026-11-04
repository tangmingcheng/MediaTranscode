#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
#include <optional>

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {

struct MediaOwnedVideoFrame final {
    ::media::ffmpeg::FramePtr frame;
    std::optional<MediaVideoFrameReadiness> readiness;
};
class FFmpegFrameBuffer : public MediaBuffer {
public:
    explicit FFmpegFrameBuffer(::media::ffmpeg::FramePtr frame);

    MediaBufferType type() const noexcept override;

    AVFrame* frame() noexcept;
    const AVFrame* frame() const noexcept;
    MediaOwnedVideoFrame takeFrame() noexcept;
    ::media::Status setReadiness(MediaVideoFrameReadiness readiness);
    const MediaVideoFrameReadiness* readiness() const noexcept
    { return m_readiness ? &*m_readiness : nullptr; }

protected:
    std::optional<MediaVideoFrameReadiness> m_readiness;
    ::media::ffmpeg::FramePtr m_frame;
};

} // namespace media::ffmpeg::graph
