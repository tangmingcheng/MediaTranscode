#pragma once

#include "internal/FFmpegUtils.h"

namespace media::ffmpeg {

    int normalizeEvenSize(int value);
    AVPixelFormat chooseVideoEncoderPixelFormat(const AVCodec* encoder);

} // namespace media::ffmpeg
