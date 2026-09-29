#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}
#include <im2d.h>
#include <libdrm/drm_fourcc.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* text)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text)); }
::media::Status rgaStatus(IM_STATUS code, const char* operation)
{
    if (code == IM_STATUS_SUCCESS || code == IM_STATUS_NOERROR) return ::media::Status::success();
    return ::media::Status::failure(::media::ErrorInfo::make(::media::ErrorCode::HardwareUnavailable,
        operation, static_cast<int>(code)));
}
struct ImportedBuffer final {
    rga_buffer_handle_t handle = 0;
    rga_buffer_t image{};
    ~ImportedBuffer() { if (handle) releasebuffer_handle(handle); }
    ImportedBuffer() = default;
    ImportedBuffer(const ImportedBuffer&) = delete;
    ImportedBuffer& operator=(const ImportedBuffer&) = delete;
    ::media::Status release()
    {
        const auto result = releasebuffer_handle(handle);
        if (result == IM_STATUS_SUCCESS || result == IM_STATUS_NOERROR) handle = 0;
        return rgaStatus(result, "RGA canvas release imported buffer");
    }
};
class CpuMapping final {
public:
    explicit CpuMapping(const AVDRMObjectDescriptor& object)
        : object_(object), address_(mmap(nullptr, object.size, PROT_READ | PROT_WRITE,
            MAP_SHARED, object.fd, 0)) {}
    ~CpuMapping() { if (address_ != MAP_FAILED) munmap(address_, object_.size); }
    CpuMapping(const CpuMapping&) = delete;
    CpuMapping& operator=(const CpuMapping&) = delete;
    void* address() const { return address_; }
    int close() { const int rc = munmap(address_, object_.size); address_ = MAP_FAILED; return rc; }
private:
    const AVDRMObjectDescriptor& object_;
    void* address_;
};
bool hasEquivalentLinearFormat(AVPixelFormat format, std::uint32_t fourcc)
{
    // Byte-addressed formats shared by FFmpeg RKMPP and RGA. Compact 10-bit
    // formats require a different pixel-stride model and are not inferred here.
    switch (format) {
    case AV_PIX_FMT_GRAY8: return fourcc == DRM_FORMAT_R8;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P: return fourcc == DRM_FORMAT_YUV420;
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P: return fourcc == DRM_FORMAT_YUV422;
    case AV_PIX_FMT_NV12: return fourcc == DRM_FORMAT_NV12;
    case AV_PIX_FMT_NV21: return fourcc == DRM_FORMAT_NV21;
    case AV_PIX_FMT_NV16: return fourcc == DRM_FORMAT_NV16;
    case AV_PIX_FMT_NV24: return fourcc == DRM_FORMAT_NV24;
    case AV_PIX_FMT_NV42: return fourcc == DRM_FORMAT_NV42;
    case AV_PIX_FMT_YUYV422: return fourcc == DRM_FORMAT_YUYV;
    case AV_PIX_FMT_YVYU422: return fourcc == DRM_FORMAT_YVYU;
    case AV_PIX_FMT_UYVY422: return fourcc == DRM_FORMAT_UYVY;
    case AV_PIX_FMT_RGB565LE: return fourcc == DRM_FORMAT_RGB565;
    case AV_PIX_FMT_BGR565LE: return fourcc == DRM_FORMAT_BGR565;
    // DRM packed RGB names describe the little-endian word, not byte order.
    case AV_PIX_FMT_RGB24: return fourcc == DRM_FORMAT_BGR888;
    case AV_PIX_FMT_BGR24: return fourcc == DRM_FORMAT_RGB888;
    case AV_PIX_FMT_RGBA: return fourcc == DRM_FORMAT_ABGR8888;
    case AV_PIX_FMT_RGB0: return fourcc == DRM_FORMAT_XBGR8888;
    case AV_PIX_FMT_BGRA: return fourcc == DRM_FORMAT_ARGB8888;
    case AV_PIX_FMT_BGR0: return fourcc == DRM_FORMAT_XRGB8888;
    case AV_PIX_FMT_ARGB: return fourcc == DRM_FORMAT_BGRA8888;
    case AV_PIX_FMT_0RGB: return fourcc == DRM_FORMAT_BGRX8888;
    case AV_PIX_FMT_ABGR: return fourcc == DRM_FORMAT_RGBA8888;
    case AV_PIX_FMT_0BGR: return fourcc == DRM_FORMAT_RGBX8888;
    default: return false;
    }
}
::media::Status describe(const AVFrame& frame, ImportedBuffer& imported)
{
    if (frame.format != AV_PIX_FMT_DRM_PRIME || !frame.data[0] || !frame.hw_frames_ctx ||
        !frame.hw_frames_ctx->data || frame.width <= 0 || frame.height <= 0)
        return invalid("RGA canvas requires DRM hardware frames");
    const auto* drm = reinterpret_cast<const AVDRMFrameDescriptor*>(frame.data[0]);
    const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    const auto* pixel = av_pix_fmt_desc_get(pool->sw_format);
    if (!pixel || drm->nb_objects != 1 || drm->nb_layers != 1 ||
        drm->objects[0].format_modifier != 0 || drm->objects[0].fd < 0 ||
        drm->objects[0].size > std::numeric_limits<int>::max())
        return invalid("RGA canvas requires a linear single-object DRM layout");
    const auto& layer = drm->layers[0];
    if (!hasEquivalentLinearFormat(pool->sw_format, layer.format))
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "RGA canvas DRM/software format has no proven linear layout mapping"));
    if (layer.nb_planes != av_pix_fmt_count_planes(pool->sw_format) || layer.nb_planes <= 0 ||
        layer.nb_planes > AV_DRM_MAX_PLANES || layer.planes[0].offset != 0 ||
        pixel->comp[0].step <= 0 || layer.planes[0].pitch <= 0 ||
        layer.planes[0].pitch % pixel->comp[0].step)
        return invalid("RGA canvas DRM plane geometry is invalid");
    const auto pixelStride = layer.planes[0].pitch / pixel->comp[0].step;
    auto heightStride = static_cast<ptrdiff_t>(frame.height);
    if (layer.nb_planes > 1) {
        if (layer.planes[1].offset <= 0 || layer.planes[1].offset % layer.planes[0].pitch)
            return invalid("RGA canvas chroma offset has no integral height stride");
        heightStride = layer.planes[1].offset / layer.planes[0].pitch;
    }
    if (pixelStride < frame.width || pixelStride > std::numeric_limits<int>::max() ||
        heightStride < frame.height || heightStride > std::numeric_limits<int>::max())
        return invalid("RGA canvas stride is outside representable image geometry");
    // RGA receives only one pixel width/height stride. Reconstruct every
    // implicit plane with FFmpeg's format algorithm before discarding the DRM
    // pitches/offsets. Subsampled dimensions must be integral, not rounded.
    if (pixelStride % (1 << pixel->log2_chroma_w) || heightStride % (1 << pixel->log2_chroma_h))
        return invalid("RGA canvas stride cannot represent integral chroma geometry");
    int lineBytes[4]{};
    ptrdiff_t pitches[4]{};
    size_t sizes[4]{};
    if (av_image_fill_linesizes(lineBytes, pool->sw_format, static_cast<int>(pixelStride)) < 0)
        return invalid("RGA canvas implicit plane strides are not representable");
    for (int index = 0; index < 4; ++index) pitches[index] = lineBytes[index];
    if (av_image_fill_plane_sizes(sizes, pool->sw_format, static_cast<int>(heightStride), pitches) < 0)
        return invalid("RGA canvas implicit plane sizes are not representable");
    size_t offset = 0;
    for (int index = 0; index < layer.nb_planes; ++index) {
        const auto& plane = layer.planes[index];
        if (plane.object_index != 0 || plane.offset < 0 ||
            static_cast<size_t>(plane.offset) != offset || plane.pitch != pitches[index] ||
            lineBytes[index] <= 0 || sizes[index] == 0)
            return invalid("RGA canvas DRM planes differ from its implicit contiguous layout");
        if (offset > drm->objects[0].size || sizes[index] > drm->objects[0].size - offset)
            return invalid("RGA canvas implicit plane extends beyond its DRM object");
        offset += sizes[index];
    }
    imported.handle = importbuffer_fd(drm->objects[0].fd, static_cast<int>(drm->objects[0].size));
    if (!imported.handle) return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable("RGA canvas importbuffer_fd failed"));
    // The deployed public overload maps DRM fourcc and modifier, including bit depth.
    imported.image = wrapbuffer_handle(imported.handle, frame.width, frame.height,
        layer.format, drm->objects[0].format_modifier, static_cast<int>(pixelStride), static_cast<int>(heightStride));
    return ::media::Status::success();
}
class RgaCanvasAdapter final : public MediaVideoCanvasAdapter {
public:
    explicit RgaCanvasAdapter(AVBufferRef* frames) : frames_(makeBufferRef(frames)) {}
    ::media::Status validate(const AVFrame& source, const AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle) override
    { return process(source, target, rectangle, false); }
    ::media::Status copy(const AVFrame& source, AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle) override
    { return process(source, target, rectangle, true); }
    ::media::Status upload(const AVFrame& source, AVFrame& target) override
    { return transfer(source, target, false); }
    ::media::Status download(const AVFrame& source, AVFrame& target) override
    { return transfer(source, target, true); }
private:
    ::media::Status transfer(const AVFrame& source, AVFrame& target, bool download)
    {
        const auto& host = download ? target : source;
        const auto& hardware = download ? source : target;
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (host.hw_frames_ctx || host.format != pool->sw_format || source.width != target.width ||
            source.height != target.height) return invalid("RGA canvas staging differs from production pool");
        ImportedBuffer checked;
        auto status = describe(hardware, checked);
        if (!status) return status;
        status = checked.release();
        if (!status) return status;
        const auto* drm = reinterpret_cast<const AVDRMFrameDescriptor*>(hardware.data[0]);
        const auto& object = drm->objects[0];
        CpuMapping mapping(object);
        if (mapping.address() == MAP_FAILED) return ::media::Status::failure(::media::ErrorInfo::ioFailure("canvas DRM mmap", errno));
        dma_buf_sync begin{};
        begin.flags = DMA_BUF_SYNC_START | (download ? DMA_BUF_SYNC_READ : DMA_BUF_SYNC_WRITE);
        if (ioctl(object.fd, DMA_BUF_IOCTL_SYNC, &begin) < 0) {
            const int error = errno;
            return ::media::Status::failure(::media::ErrorInfo::ioFailure("canvas DRM begin CPU access", error));
        }
        std::uint8_t* planes[4]{};
        int pitches[4]{};
        const auto& layer = drm->layers[0];
        for (int index = 0; index < layer.nb_planes; ++index) {
            planes[index] = static_cast<std::uint8_t*>(mapping.address()) + layer.planes[index].offset;
            pitches[index] = static_cast<int>(layer.planes[index].pitch);
        }
        const std::uint8_t* inputs[4]{};
        for (int index = 0; index < 4; ++index) inputs[index] = download ? planes[index] : host.data[index];
        if (download) av_image_copy(target.data, target.linesize, inputs, pitches,
            pool->sw_format, source.width, source.height);
        else av_image_copy(planes, pitches, inputs, source.linesize,
            pool->sw_format, source.width, source.height);
        dma_buf_sync end{};
        end.flags = DMA_BUF_SYNC_END | (download ? DMA_BUF_SYNC_READ : DMA_BUF_SYNC_WRITE);
        const int syncResult = ioctl(object.fd, DMA_BUF_IOCTL_SYNC, &end);
        const int syncError = errno;
        const int unmapResult = mapping.close();
        if (syncResult < 0) return ::media::Status::failure(::media::ErrorInfo::ioFailure("canvas DRM end CPU access", syncError));
        if (unmapResult < 0) return ::media::Status::failure(::media::ErrorInfo::ioFailure("canvas DRM munmap", errno));
        return ::media::Status::success();
    }
    ::media::Status process(const AVFrame& source, const AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle, bool execute)
    {
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (!source.hw_frames_ctx || !target.hw_frames_ctx ||
            reinterpret_cast<const AVHWFramesContext*>(source.hw_frames_ctx->data)->sw_format != pool->sw_format ||
            reinterpret_cast<const AVHWFramesContext*>(target.hw_frames_ctx->data)->sw_format != pool->sw_format ||
            rectangle.width != source.width || rectangle.height != source.height || rectangle.x < 0 || rectangle.y < 0 ||
            rectangle.width > target.width || rectangle.height > target.height ||
            rectangle.x > target.width - rectangle.width || rectangle.y > target.height - rectangle.height)
            return invalid("RGA canvas tile format or rectangle differs from plan");
        ImportedBuffer src, dst;
        auto status = describe(source, src);
        if (!status) return status;
        status = describe(target, dst);
        if (!status) return status;
        const im_rect srcRect{0, 0, rectangle.width, rectangle.height};
        const im_rect dstRect{rectangle.x, rectangle.y, rectangle.width, rectangle.height};
        status = rgaStatus(imcheck_t(src.image, dst.image, {}, srcRect, dstRect, {}, IM_SYNC), "RGA canvas capability check");
        if (status && execute)
            status = rgaStatus(improcess(src.image, dst.image, {}, srcRect, dstRect, {}, IM_SYNC), "RGA canvas synchronous rectangle copy");
        const auto releaseSource = src.release();
        const auto releaseTarget = dst.release();
        if (!status) return status;
        if (!releaseSource) return releaseSource;
        return releaseTarget;
    }
    BufferRefPtr frames_;
};
}
::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> createMediaRgaCanvasAdapter(AVBufferRef* frames)
{
    return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::success(std::make_unique<RgaCanvasAdapter>(frames));
}
} // namespace media::ffmpeg::graph
