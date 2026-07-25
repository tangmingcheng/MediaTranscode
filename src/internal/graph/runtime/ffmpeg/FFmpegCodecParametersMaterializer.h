#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class FFmpegCodecParametersMaterializer final {
public:
    static ::media::Result<::media::ffmpeg::CodecParametersPtr> fromContext(
        const AVCodecContext& context);

private:
    FFmpegCodecParametersMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
