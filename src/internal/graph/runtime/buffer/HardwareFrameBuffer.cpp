#include "internal/graph/runtime/buffer/HardwareFrameBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

HardwareFrameBuffer::HardwareFrameBuffer(::media::ffmpeg::FramePtr frame, MediaHardwareDescriptor hardware)
    : FFmpegFrameBuffer(std::move(frame))
{
    setHardwareDescriptor(std::move(hardware));
    addFlags(MediaBufferFlag::HardwareBacked);
}

MediaBufferType HardwareFrameBuffer::type() const noexcept
{
    return MediaBufferType::HardwareFrame;
}

} // namespace media::ffmpeg::graph
