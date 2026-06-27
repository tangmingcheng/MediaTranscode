#pragma once

#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}

namespace media::ffmpeg::graph {

class FFmpegDescriptorMapper final {
public:
    static MediaStreamKind toStreamKind(AVMediaType type) noexcept;
    static MediaCodecDomain toCodecDomain(AVMediaType type) noexcept;
    static MediaRational toRational(AVRational rational) noexcept;

    static MediaFormatDescriptor fromStream(const AVStream* stream);
    static MediaFormatDescriptor fromCodecParameters(const AVCodecParameters* params);
    static MediaFormatDescriptor fromCodecContext(const AVCodecContext* context,
                                                  MediaCodecOperation operation = MediaCodecOperation::Unknown);
    static MediaFormatDescriptor fromFrame(const AVFrame* frame, MediaStreamKind streamKind);

    static MediaHardwareDeviceKind toHardwareDeviceKind(AVHWDeviceType type) noexcept;
    static MediaHardwareFrameKind toHardwareFrameKind(const AVFrame* frame) noexcept;
};

} // namespace media::ffmpeg::graph
