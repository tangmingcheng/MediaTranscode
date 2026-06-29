#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg::graph {

::media::Status configureEncoderHardwareFrames(AVCodecContext* encoderContext,
                                               AVBufferRef* hardwareDevice,
                                               AVPixelFormat hardwareFormat,
                                               AVPixelFormat softwareFormat,
                                               int width,
                                               int height,
                                               int initialPoolSize);

} // namespace media::ffmpeg::graph
