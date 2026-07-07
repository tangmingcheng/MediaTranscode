#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {

class FFmpegFrameBuffer : public MediaBuffer {
public:
    explicit FFmpegFrameBuffer(::media::ffmpeg::FramePtr frame);

    MediaBufferType type() const noexcept override;

    AVFrame* frame() noexcept;
    const AVFrame* frame() const noexcept;
    ::media::ffmpeg::FramePtr takeFrame() noexcept;

protected:
    ::media::ffmpeg::FramePtr m_frame;
};

} // namespace media::ffmpeg::graph
