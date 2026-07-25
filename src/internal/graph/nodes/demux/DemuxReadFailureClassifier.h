#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

::media::Status classifyDemuxReadFailure(int ffmpegCode, bool interruptRequested);

} // namespace media::ffmpeg::graph
