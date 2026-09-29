#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
#include <ffnvcodec/dynlink_cuda.h>
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <string>

namespace media::ffmpeg::graph {
namespace {
class CudaLibrary final {
public:
    CudaLibrary() : handle_(LoadLibraryExW(L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {}
    ~CudaLibrary() { if (handle_) FreeLibrary(handle_); }
    CudaLibrary(const CudaLibrary&) = delete;
    CudaLibrary& operator=(const CudaLibrary&) = delete;
    template<class T> T* symbol(const char* name) const
    { return handle_ ? reinterpret_cast<T*>(GetProcAddress(handle_, name)) : nullptr; }
private:
    HMODULE handle_;
};
::media::Status cudaStatus(CUresult code, const char* operation)
{
    if (code == CUDA_SUCCESS) return ::media::Status::success();
    return ::media::Status::failure(::media::ErrorInfo::make(
        ::media::ErrorCode::HardwareUnavailable, operation, static_cast<int>(code)));
}
const AVCUDADeviceContext* device(const AVFrame& frame)
{
    if (!frame.hw_frames_ctx || !frame.hw_frames_ctx->data) return nullptr;
    const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    if (!pool->device_ctx || pool->device_ctx->type != AV_HWDEVICE_TYPE_CUDA) return nullptr;
    return static_cast<const AVCUDADeviceContext*>(pool->device_ctx->hwctx);
}
class CudaCanvasAdapter final : public MediaVideoCanvasAdapter {
public:
    explicit CudaCanvasAdapter(AVBufferRef* frames) : frames_(makeBufferRef(frames))
    {
        push_ = library_.symbol<tcuCtxPushCurrent_v2>("cuCtxPushCurrent_v2");
        pop_ = library_.symbol<tcuCtxPopCurrent_v2>("cuCtxPopCurrent_v2");
        copy_ = library_.symbol<tcuMemcpy2DAsync_v2>("cuMemcpy2DAsync_v2");
        upload_ = library_.symbol<tcuMemcpy2D_v2>("cuMemcpy2D_v2");
        sync_ = library_.symbol<tcuStreamSynchronize>("cuStreamSynchronize");
    }
    bool ready() const { return frames_ && push_ && pop_ && copy_ && upload_ && sync_; }
    ::media::Status validate(const AVFrame& source, const AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle) override
    {
        const auto* srcDevice = device(source);
        const auto* dstDevice = device(target);
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (!srcDevice || !dstDevice || srcDevice->cuda_ctx != dstDevice->cuda_ctx ||
            dstDevice->cuda_ctx != static_cast<AVCUDADeviceContext*>(pool->device_ctx->hwctx)->cuda_ctx ||
            source.format != AV_PIX_FMT_CUDA || target.format != AV_PIX_FMT_CUDA ||
            reinterpret_cast<const AVHWFramesContext*>(source.hw_frames_ctx->data)->sw_format != pool->sw_format ||
            reinterpret_cast<const AVHWFramesContext*>(target.hw_frames_ctx->data)->sw_format != pool->sw_format ||
            source.width != rectangle.width || source.height != rectangle.height ||
            rectangle.x < 0 || rectangle.y < 0 || rectangle.width > target.width || rectangle.height > target.height ||
            rectangle.x > target.width - rectangle.width || rectangle.y > target.height - rectangle.height)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA canvas source/context/geometry differs from plan"));
        return ::media::Status::success();
    }
    ::media::Status upload(const AVFrame& source, AVFrame& target) override
    {
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (source.hw_frames_ctx || source.format != pool->sw_format ||
            source.width != target.width || source.height != target.height)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA canvas upload differs from pool"));
        return transfer(source, target, {0, 0, source.width, source.height}, true, false);
    }
    ::media::Status download(const AVFrame& source, AVFrame& target) override
    {
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (target.hw_frames_ctx || target.format != pool->sw_format ||
            source.width != target.width || source.height != target.height)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA canvas readback differs from pool"));
        return transfer(source, target, {0, 0, source.width, source.height}, false, true);
    }
    ::media::Status copy(const AVFrame& source, AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle) override
    {
        auto status = validate(source, target, rectangle);
        if (!status) return status;
        return transfer(source, target, rectangle, false, false);
    }
private:
    ::media::Status transfer(const AVFrame& source, AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle, bool host, bool download)
    {
        const auto* dstDevice = device(download ? source : target);
        if (!dstDevice) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA target lacks device"));
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        const auto* descriptor = av_pix_fmt_desc_get(pool->sw_format);
        if (!descriptor) return ::media::Status::failure(::media::ErrorInfo::unsupported("CUDA canvas pixel descriptor unavailable"));
        auto status = cudaStatus(push_(dstDevice->cuda_ctx), "CUDA canvas context push");
        if (!status) return status;
        // The source may be produced by a different stream in the same context.
        if (!host) status = cudaStatus(sync_(device(source)->stream), "CUDA canvas source completion");
        const int planes = av_pix_fmt_count_planes(pool->sw_format);
        for (int plane = 0; status && plane < planes; ++plane) {
            bool chroma = false;
            for (int component = 1; component < 3 && component < descriptor->nb_components; ++component)
                chroma |= descriptor->comp[component].plane == plane;
            chroma &= !(descriptor->flags & AV_PIX_FMT_FLAG_RGB) && descriptor->comp[0].plane != plane;
            const int shift = chroma ? descriptor->log2_chroma_h : 0;
            const int width = av_image_get_linesize(pool->sw_format, source.width, plane);
            const int offset = rectangle.x ? av_image_get_linesize(pool->sw_format, rectangle.x, plane) : 0;
            const int rows = (source.height + (1 << shift) - 1) >> shift;
            if (width <= 0 || offset < 0 || !source.data[plane] || !target.data[plane] ||
                source.linesize[plane] < width || target.linesize[plane] < offset + width) {
                status = ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA canvas plane stride is invalid"));
                break;
            }
            CUDA_MEMCPY2D operation{};
            operation.srcMemoryType = host ? CU_MEMORYTYPE_HOST : CU_MEMORYTYPE_DEVICE;
            operation.srcHost = host ? source.data[plane] : nullptr;
            operation.srcDevice = host ? 0 : reinterpret_cast<CUdeviceptr>(source.data[plane]);
            operation.srcPitch = source.linesize[plane];
            operation.dstMemoryType = download ? CU_MEMORYTYPE_HOST : CU_MEMORYTYPE_DEVICE;
            operation.dstHost = download ? target.data[plane] : nullptr;
            operation.dstDevice = download ? 0 : reinterpret_cast<CUdeviceptr>(target.data[plane]);
            operation.dstPitch = target.linesize[plane];
            operation.dstXInBytes = offset;
            operation.dstY = rectangle.y >> shift;
            operation.WidthInBytes = width;
            operation.Height = rows;
            status = cudaStatus((host || download) ? upload_(&operation) : copy_(&operation, dstDevice->stream),
                "CUDA canvas plane copy");
        }
        // Even a partial submission must finish before either frame can be released.
        const auto completion = cudaStatus(sync_(dstDevice->stream), "CUDA canvas destination completion");
        CUcontext popped = nullptr;
        const auto restored = cudaStatus(pop_(&popped), "CUDA canvas context pop");
        if (!status) return status;
        if (!completion) return completion;
        if (!restored) return restored;
        if (popped != dstDevice->cuda_ctx)
            return ::media::Status::failure(::media::ErrorInfo::internalError("CUDA canvas context stack mismatch"));
        return ::media::Status::success();
    }
    CudaLibrary library_;
    BufferRefPtr frames_;
    tcuCtxPushCurrent_v2* push_ = nullptr;
    tcuCtxPopCurrent_v2* pop_ = nullptr;
    tcuMemcpy2DAsync_v2* copy_ = nullptr;
    tcuMemcpy2D_v2* upload_ = nullptr;
    tcuStreamSynchronize* sync_ = nullptr;
};
}
::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> createMediaCudaCanvasAdapter(AVBufferRef* frames)
{
    auto adapter = std::make_unique<CudaCanvasAdapter>(frames);
    if (!adapter->ready()) return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(
        ::media::ErrorInfo::hardwareUnavailable("CUDA canvas driver exports unavailable"));
    return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::success(std::move(adapter));
}
} // namespace media::ffmpeg::graph
