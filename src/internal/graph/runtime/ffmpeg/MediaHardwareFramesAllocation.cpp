#include "internal/graph/runtime/ffmpeg/MediaHardwareFramesAllocation.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

#include <utility>

namespace media::ffmpeg::graph {
::media::Result<::media::ffmpeg::BufferRefPtr> allocateHardwareFramesContext(
    AVBufferRef* device, AVPixelFormat hardwareFormat, AVPixelFormat softwareFormat,
    int width, int height, int initialPoolSize)
{
    using Result = ::media::Result<::media::ffmpeg::BufferRefPtr>;
    if (!device || hardwareFormat == AV_PIX_FMT_NONE || softwareFormat == AV_PIX_FMT_NONE ||
        width <= 0 || height <= 0 || initialPoolSize <= 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Hardware frames allocation requires device, formats, geometry and positive pool count"));
    ::media::ffmpeg::BufferRefPtr frames(av_hwframe_ctx_alloc(device));
    if (!frames) return Result::failure(::media::ErrorInfo::allocationFailed("av_hwframe_ctx_alloc"));
    auto* context = reinterpret_cast<AVHWFramesContext*>(frames->data);
    context->format = hardwareFormat;
    context->sw_format = softwareFormat;
    context->width = width;
    context->height = height;
    context->initial_pool_size = initialPoolSize;
    const int code = av_hwframe_ctx_init(frames.get());
    if (code < 0) return Result::failure(FFmpegGraphError::fromCode(code, "av_hwframe_ctx_init"));
    return Result::success(std::move(frames));
}
} // namespace media::ffmpeg::graph
