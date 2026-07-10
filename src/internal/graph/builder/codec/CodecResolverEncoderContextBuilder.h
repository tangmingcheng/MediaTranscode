#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;
struct AVCodecParameters;

namespace media::ffmpeg::graph {

struct CodecResolverEncoderContextBuildRequest {
    const AVCodecParameters* codecParameters = nullptr;
    MediaFormatDescriptor sourceFormat;
    MediaTimeDescriptor sourceTime;
    const MediaNodeOptions* options = nullptr;
    AVBufferRef* hardwareDevice = nullptr;
};

struct CodecResolverEncoderContextBuildResult {
    ::media::ffmpeg::CodecContextPtr context;
    AVPixelFormat hardwareFramesFormat = AV_PIX_FMT_NONE;
    AVPixelFormat surfaceSoftwareFormat = AV_PIX_FMT_NONE;
};

class CodecResolverEncoderContextBuilder final {
public:
    static ::media::Result<CodecResolverEncoderContextBuildResult> build(
        const CodecResolverEncoderContextBuildRequest& request);

private:
    CodecResolverEncoderContextBuilder() = default;
};

} // namespace media::ffmpeg::graph
