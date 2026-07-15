#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

[[nodiscard]] ::media::Status validateMediaFfmpegCopyOpaqueCapability(
    bool compileTimeFlagAvailable,
    unsigned compileTimeMajor,
    unsigned runtimeMajor);

[[nodiscard]] ::media::Status requireMediaFfmpegCopyOpaqueCapability();

} // namespace media::ffmpeg::graph
