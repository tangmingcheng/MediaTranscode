#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"

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

::media::Result<MediaBufferRef> FFmpegCodecParametersMaterializer::snapshot(const AVCodecContext& context)
{
    if ((context.codec_type != AVMEDIA_TYPE_VIDEO && context.codec_type != AVMEDIA_TYPE_AUDIO) ||
        context.time_base.num <= 0 || context.time_base.den <= 0)
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
            "encoder metadata snapshot requires an opened audio/video codec and explicit time base"));
    auto parameters = fromContext(context);
    if (!parameters) return ::media::Result<MediaBufferRef>::failure(parameters.error());
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(std::move(parameters).value());
    buffer->setStreamKind(context.codec_type == AVMEDIA_TYPE_VIDEO ? MediaStreamKind::Video : MediaStreamKind::Audio);
    MediaTimeDescriptor time;
    time.timeBase = MediaRational{context.time_base.num, context.time_base.den};
    buffer->setTimeDescriptor(time);
    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

} // namespace media::ffmpeg::graph
