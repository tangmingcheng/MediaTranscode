#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
#include "internal/graph/diagnostics/MediaCanvasTransferObservation.h"
#include "internal/graph/diagnostics/MediaCanvasComposeObservation.h"
#include "internal/graph/runtime/ffmpeg/platform/MediaCudaCanvasResources.h"
#include "internal/graph/runtime/ffmpeg/platform/MediaCudaCanvasTiming.h"
#include <ffnvcodec/dynlink_cuda.h>
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}
#include <array>
#include <string>

namespace media::ffmpeg::graph {
namespace {
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
    ::media::Status prepareStream()
    {
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        const auto* hardware = static_cast<const AVCUDADeviceContext*>(pool->device_ctx->hwctx);
        auto prepared = stream_.initialize(library_, hardware->cuda_ctx);
        if (!prepared) return prepared;
        auto timingPrepared = timing_.initialize(library_, hardware->cuda_ctx);
        if (!timingPrepared) return timingPrepared;
        try {
            std::ostringstream out;
            out << "canvas_cuda_stream stage=prepared device_stream=" << hardware->stream
                << " private_nonblocking=1";
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::PlannerCapability, out.str());
        } catch (...) {}
        return ::media::Status::success();
    }
    ~CudaCanvasAdapter() override { observation_.log(); }
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
        return transfer(source, target, {0, 0, source.width, source.height}, true, false,
            MediaVideoFrameReadiness::completed(), nullptr);
    }
    ::media::Status download(const AVFrame& source, AVFrame& target) override
    {
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        if (target.hw_frames_ctx || target.format != pool->sw_format ||
            source.width != target.width || source.height != target.height)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA canvas readback differs from pool"));
        return transfer(source, target, {0, 0, source.width, source.height}, false, true,
            MediaVideoFrameReadiness::completed(), nullptr);
    }
    ::media::Status executeBatch(const MediaVideoCanvasCopyBatch& batch) override
    {
        return transfer(batch.black, batch.target, batch.full, false, false,
            MediaVideoFrameReadiness::completed(), &batch);
    }
private:
    ::media::Status transfer(const AVFrame& source, AVFrame& target,
        const MediaVideoCanvasRectangle& rectangle, bool host, bool download,
        const MediaVideoFrameReadiness& readiness, const MediaVideoCanvasCopyBatch* batch)
    {
        const auto* dstDevice = device(download ? source : target);
        if (!dstDevice) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA target lacks device"));
        const auto* pool = reinterpret_cast<const AVHWFramesContext*>(frames_->data);
        const auto* descriptor = av_pix_fmt_desc_get(pool->sw_format);
        if (!descriptor) return ::media::Status::failure(::media::ErrorInfo::unsupported("CUDA canvas pixel descriptor unavailable"));
        using Stage = MediaCanvasTransferObservation::Stage;
        using Operation = MediaCanvasTransferObservation::Operation;
        const auto operationKind = host ? Operation::Upload : download ? Operation::Download : Operation::Copy;
        cuda_canvas::ContextScope context(push_, pop_);
        auto status = cuda_canvas::status(observation_.measure(Stage::ContextPush, operationKind,
            [&] { return MediaCanvasComposeObservation::measure(MediaCanvasComposeObservation::Stage::ContextPush,
                [&] { return context.open(dstDevice->cuda_ctx); }); }), "CUDA canvas context push");
        if (!status) return status;
        // cuMemcpy2D_v2 uses the legacy default stream. Pageable uploads may
        // return before DMA completes; the private stream cannot fence them.
        cuda_canvas::CompletionScope pending(sync_, (host || download) ? nullptr : stream_.get());
        const auto enqueue = [&](const AVFrame& source, const MediaVideoCanvasRectangle& rectangle,
            const MediaVideoFrameReadiness& readiness) -> ::media::Status {
            auto status = ::media::Status::success();
            if (!host && !download) {
                MediaCanvasComposeObservation::observeReadiness(
                    [&] { return readiness.observeCompletion(); });
                status = observation_.measure(Stage::SourceWait, operationKind, [&] {
                    return MediaCanvasComposeObservation::measure(MediaCanvasComposeObservation::Stage::SourceWait,
                        [&] { return readiness.enqueueCudaWait(dstDevice->cuda_ctx, stream_.get()); });
                });
            }
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
                status = cuda_canvas::status(observation_.measure(Stage::PlaneCopy, operationKind,
                    [&] { return MediaCanvasComposeObservation::measure(MediaCanvasComposeObservation::Stage::PlaneCopy,
                    [&] { return (host || download) ? upload_(&operation) : copy_(&operation, stream_.get()); }); }),
                    "CUDA canvas plane copy");
            }
            return status;
        };
        if (download) {
            status = cuda_canvas::status(observation_.measure(Stage::SourceWait, operationKind, [&] {
                return MediaCanvasComposeObservation::measure(MediaCanvasComposeObservation::Stage::SourceWait,
                    [&] { return sync_(device(source)->stream); });
            }), "CUDA canvas readback source completion");
        }
        std::optional<MediaCanvasBatchTiming> timing;
        if (batch && MediaCanvasComposeObservation::active()) {
            const auto begin = mediaSteadyClockNowNs();
            timing = timing_.begin(stream_.get());
            timing->submitBegin = begin;
        }
        if (status) status = batch ? batch->visit(enqueue) : enqueue(source, rectangle, readiness);
        if (timing) {
            timing_.end(stream_.get(), *timing);
            timing->submitEnd = mediaSteadyClockNowNs();
        }
        // Even a partial submission must finish before either frame can be released.
        const auto completion = cuda_canvas::status(observation_.measure(batch ? Stage::BatchCompletion : Stage::DestinationWait, operationKind,
            [&] { return MediaCanvasComposeObservation::measure(batch ? MediaCanvasComposeObservation::Stage::BatchCompletion : MediaCanvasComposeObservation::Stage::DestinationWait,
                [&] {
                    if (timing) timing->waitBegin = mediaSteadyClockNowNs();
                    const auto code = pending.complete();
                    if (timing) {
                        timing->waitEnd = mediaSteadyClockNowNs();
                        timing->completionResult = static_cast<int>(code);
                    }
                    return code;
                }); }), "CUDA canvas destination completion");
        if (timing) {
            timing_.read(*timing);
            MediaCanvasComposeObservation::recordBatchTiming(*timing);
        }
        const auto restored = cuda_canvas::status(observation_.measure(Stage::ContextPop, operationKind,
            [&] { return MediaCanvasComposeObservation::measure(MediaCanvasComposeObservation::Stage::ContextPop,
                [&] { return context.close(); }); }), "CUDA canvas context pop");
        if (!status && !completion)
            cuda_canvas::reportCleanup(static_cast<CUresult>(completion.error().nativeCode), "copy_secondary_completion");
        if ((!status || !completion) && !restored)
            cuda_canvas::reportCleanup(static_cast<CUresult>(restored.error().nativeCode), "copy_secondary_context_pop");
        if (!status) return status;
        if (!completion) return completion;
        if (!restored) return restored;
        return ::media::Status::success();
    }
    cuda_canvas::Library library_;
    MediaCanvasTransferObservation observation_;
    BufferRefPtr frames_;
    tcuCtxPushCurrent_v2* push_ = nullptr;
    tcuCtxPopCurrent_v2* pop_ = nullptr;
    tcuMemcpy2DAsync_v2* copy_ = nullptr;
    tcuMemcpy2D_v2* upload_ = nullptr;
    tcuStreamSynchronize* sync_ = nullptr;
    cuda_canvas::OwnedStream stream_;
    cuda_canvas::BatchTiming timing_;
};
}
::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> createMediaCudaCanvasAdapter(AVBufferRef* frames)
{
    auto adapter = std::make_unique<CudaCanvasAdapter>(frames);
    if (!adapter->ready()) return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(
        ::media::ErrorInfo::hardwareUnavailable("CUDA canvas driver exports unavailable"));
    auto prepared = adapter->prepareStream();
    if (!prepared) return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::failure(prepared.error());
    return ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>>::success(std::move(adapter));
}
} // namespace media::ffmpeg::graph
