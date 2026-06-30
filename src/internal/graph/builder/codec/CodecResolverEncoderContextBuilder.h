#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;
struct AVFormatContext;
struct AVStream;

namespace media::ffmpeg::graph {

struct CodecResolverEncoderContextBuildRequest {
    AVFormatContext* formatContext = nullptr;
    AVStream* stream = nullptr;
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
