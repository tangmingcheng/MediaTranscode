#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/imgutils.h>
}
#include <limits>

namespace media::ffmpeg::graph {
#if defined(_WIN32)
::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> createMediaCudaCanvasAdapter(AVBufferRef* frames);
#endif
#if defined(MT_HAVE_RGA_CANVAS)
::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> createMediaRgaCanvasAdapter(AVBufferRef* frames);
#endif

::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> MediaVideoCanvasAdapter::create(AVBufferRef* frames)
{
    if (!frames || !frames->data)
        return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(
            ::media::ErrorInfo::invalidArgument("canvas adapter requires production frames"));
    const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames->data);
    if (!pool->device_ctx || !pool->device_ctx->hwctx)
        return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(
            ::media::ErrorInfo::invalidArgument("canvas production frames have no hardware device context"));
#if defined(_WIN32)
    if (pool->format == AV_PIX_FMT_CUDA) return createMediaCudaCanvasAdapter(frames);
#endif
#if defined(MT_HAVE_RGA_CANVAS)
    if (pool->format == AV_PIX_FMT_DRM_PRIME) return createMediaRgaCanvasAdapter(frames);
#endif
    return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(
        ::media::ErrorInfo::unsupported("no completion-checked canvas adapter for production hardware format"));
}

::media::Result<MediaVideoCanvasAllocation> readMediaVideoCanvasAllocation(const AVFrame& frame)
{
    using Result = ::media::Result<MediaVideoCanvasAllocation>;
    if (!frame.hw_frames_ctx || !frame.hw_frames_ctx->data || frame.width <= 0 || frame.height <= 0)
        return Result::failure(::media::ErrorInfo::invalidArgument("canvas allocation requires production hardware frame"));
    const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    const int hostBytes = av_image_get_buffer_size(pool->sw_format, frame.width, frame.height, 1);
    if (hostBytes <= 0) return Result::failure(::media::ErrorInfo::unsupported("canvas staging geometry is unsupported"));
    std::uint64_t bytes = 0;
    if (frame.format == AV_PIX_FMT_CUDA) {
        // The public CUDA pool contract exposes its allocation in buffer size.
        for (const auto* buffer : frame.buf) {
            if (!buffer) continue;
            if (buffer->size > std::numeric_limits<std::uint64_t>::max() - bytes)
                return Result::failure(::media::ErrorInfo::invalidArgument("canvas allocation overflows"));
            bytes += buffer->size;
        }
    } else if (frame.format == AV_PIX_FMT_DRM_PRIME && frame.data[0]) {
        const auto* drm = reinterpret_cast<const AVDRMFrameDescriptor*>(frame.data[0]);
        if (drm->nb_objects <= 0 || drm->nb_objects > AV_DRM_MAX_PLANES)
            return Result::failure(::media::ErrorInfo::invalidArgument("canvas DRM object count is invalid"));
        for (int index = 0; index < drm->nb_objects; ++index) {
            if (drm->objects[index].size > std::numeric_limits<std::uint64_t>::max() - bytes)
                return Result::failure(::media::ErrorInfo::invalidArgument("canvas DRM allocation overflows"));
            bytes += drm->objects[index].size;
        }
    } else return Result::failure(::media::ErrorInfo::unsupported("canvas allocation has no authoritative byte readback"));
    if (!bytes) return Result::failure(::media::ErrorInfo::invalidArgument("canvas allocation is empty"));
    return Result::success({bytes, static_cast<std::uint64_t>(hostBytes)});
}
} // namespace media::ffmpeg::graph
