#pragma once

#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"

namespace media::ffmpeg::graph {

class HardwareFrameBuffer final : public FFmpegFrameBuffer {
public:
    HardwareFrameBuffer(::media::ffmpeg::FramePtr frame, MediaHardwareDescriptor hardware);

    MediaBufferType type() const noexcept override;
};

} // namespace media::ffmpeg::graph
