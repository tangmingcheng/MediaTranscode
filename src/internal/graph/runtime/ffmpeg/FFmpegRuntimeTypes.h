#pragma once

#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

struct FFmpegStreamBinding {
    int streamIndex = invalidMediaStreamIndex;
    AVMediaType mediaType = AVMEDIA_TYPE_UNKNOWN;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaFormatDescriptor format;
};

struct FFmpegCodecBinding {
    const AVCodec* codec = nullptr;
    AVCodecContext* context = nullptr;
    MediaCodecOperation operation = MediaCodecOperation::Unknown;
    std::string codecName;
};

struct FFmpegHardwareBinding {
    MediaHardwareDescriptor descriptor;
    AVBufferRef* deviceContext = nullptr;
    AVBufferRef* framesContext = nullptr;
};

} // namespace media::ffmpeg::graph
