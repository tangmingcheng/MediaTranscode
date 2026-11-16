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
    FFmpegFrameBuffer(::media::ffmpeg::FramePtr frame, std::uint64_t logicalPayloadBytes);

    MediaBufferType type() const noexcept override;
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;

    // Geometry is immutable after wrapping; geometry changes require a new wrapper.
    AVFrame* frame() noexcept;
    const AVFrame* frame() const noexcept;
    MediaOwnedVideoFrame takeFrame() noexcept;
    ::media::Status setReadiness(MediaVideoFrameReadiness readiness);
    const MediaVideoFrameReadiness* readiness() const noexcept
    { return m_readiness ? &*m_readiness : nullptr; }

protected:
    std::optional<MediaVideoFrameReadiness> m_readiness;
    ::media::ffmpeg::FramePtr m_frame;

private:
    const std::uint64_t m_logicalPayloadBytes;
};

} // namespace media::ffmpeg::graph
