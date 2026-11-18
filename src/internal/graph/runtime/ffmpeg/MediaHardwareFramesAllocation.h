#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

// Allocation adapter only: geometry and pool count are caller-owned contracts.
::media::Result<::media::ffmpeg::BufferRefPtr> allocateHardwareFramesContext(
    AVBufferRef* device, AVPixelFormat hardwareFormat, AVPixelFormat softwareFormat,
    int width, int height, int initialPoolSize);

} // namespace media::ffmpeg::graph
