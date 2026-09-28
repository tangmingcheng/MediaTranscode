#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaGraphTypes.h"

#include <optional>
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;

namespace media::ffmpeg::graph {

struct MediaVideoEncoderFrameInput {
    MediaRational sampleAspectRatio;
    AVColorRange colorRange;
    AVColorPrimaries colorPrimaries;
    AVColorTransferCharacteristic colorTransfer;
    AVColorSpace colorSpace;
};

struct CodecResolverEncoderContextBuildRequest {
    std::optional<MediaVideoEncoderFrameInput> frameInput;
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
