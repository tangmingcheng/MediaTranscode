#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<::media::ffmpeg::CodecParametersPtr>
FFmpegCodecParametersMaterializer::fromContext(
    const AVCodecContext& context)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) {
        return ::media::Result<
            ::media::ffmpeg::CodecParametersPtr>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "FFmpeg codec parameters"));
    }
    const int copied = avcodec_parameters_from_context(
        parameters.get(), &context);
    if (copied < 0) {
        return ::media::Result<
            ::media::ffmpeg::CodecParametersPtr>::failure(
                FFmpegGraphError::fromCode(
                    copied, "avcodec_parameters_from_context"));
    }
    return ::media::Result<
        ::media::ffmpeg::CodecParametersPtr>::success(
            std::move(parameters));
}

} // namespace media::ffmpeg::graph
