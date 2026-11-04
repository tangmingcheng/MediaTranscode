#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#ifdef _WIN32
#include "internal/graph/runtime/ffmpeg/platform/MediaCudaCanvasResources.h"
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
}
namespace media::ffmpeg::graph {
namespace {
void drainUnattachedProducer(CUcontext context, CUstream stream) noexcept
{
    cuda_canvas::Library library;
    auto* push = library.symbol<tcuCtxPushCurrent_v2>("cuCtxPushCurrent_v2");
    auto* pop = library.symbol<tcuCtxPopCurrent_v2>("cuCtxPopCurrent_v2");
    auto* drain = library.symbol<tcuStreamSynchronize>("cuStreamSynchronize");
    if (!push || !pop || !drain) {
        try {
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Summary,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "readiness_unattached_cleanup_exports unavailable; producer drain was not attempted");
        } catch (...) {}
        return;
    }
    cuda_canvas::ContextScope scope(push, pop);
    const auto opened = scope.open(context);
    if (opened != CUDA_SUCCESS) { cuda_canvas::reportCleanup(opened, "readiness_unattached_context"); return; }
    cuda_canvas::reportCleanup(drain(stream), "readiness_unattached_drain");
}
class CudaFrameReadiness final : public MediaVideoFrameReadinessOperation {
public:
    explicit CudaFrameReadiness(std::shared_ptr<MediaPreparationMemoryResource> resource, BufferRefPtr device, CUcontext context, CUstream producer,
        std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs)
        : resource_(std::move(resource)), device_(std::move(device)), consumedInputs_(std::move(consumedInputs)), context_(context), producer_(producer)
    {
        push_ = library_.symbol<tcuCtxPushCurrent_v2>("cuCtxPushCurrent_v2");
        pop_ = library_.symbol<tcuCtxPopCurrent_v2>("cuCtxPopCurrent_v2");
        create_ = library_.symbol<tcuEventCreate>("cuEventCreate");
        record_ = library_.symbol<tcuEventRecord>("cuEventRecord");
        query_ = library_.symbol<tcuEventQuery>("cuEventQuery");
        wait_ = library_.symbol<tcuStreamWaitEvent>("cuStreamWaitEvent");
        synchronize_ = library_.symbol<tcuEventSynchronize>("cuEventSynchronize");
        destroy_ = library_.symbol<tcuEventDestroy_v2>("cuEventDestroy_v2");
        drain_ = library_.symbol<tcuStreamSynchronize>("cuStreamSynchronize");
    }
    ~CudaFrameReadiness() override
    {
        if (!push_ || !pop_ || !drain_) return;
        cuda_canvas::reportCleanup(static_cast<CUresult>(complete()), "readiness_completion");
        if (!event_ || !destroy_) return;
        cuda_canvas::ContextScope scope(push_, pop_);
        const auto opened = scope.open(context_);
        if (opened != CUDA_SUCCESS) { cuda_canvas::reportCleanup(opened, "readiness_destroy_context"); return; }
        cuda_canvas::reportCleanup(destroy_(event_), "readiness_event_destroy");
    }
    ::media::Status record()
    {
        if (!push_ || !pop_ || !create_ || !record_ || !wait_ || !synchronize_ || !destroy_ || !drain_)
            return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable("CUDA readiness driver exports unavailable"));
        cuda_canvas::ContextScope scope(push_, pop_);
        auto code = scope.open(context_);
        if (code != CUDA_SUCCESS) return cuda_canvas::status(code, "CUDA readiness context push");
        code = create_(&event_, CU_EVENT_DISABLE_TIMING);
        if (code != CUDA_SUCCESS) return cuda_canvas::status(code, "CUDA readiness event create");
        code = record_(event_, producer_);
        if (code != CUDA_SUCCESS) {
            cuda_canvas::reportCleanup(static_cast<CUresult>(complete()), "readiness_record_failure_drain");
            return cuda_canvas::status(code, "CUDA readiness event record");
        }
        recorded_ = true;
        return cuda_canvas::status(scope.close(), "CUDA readiness record context pop");
    }
    MediaVideoReadinessObservation observeCompletion() noexcept override
    {
        using State = MediaVideoReadinessObservation::State;
        try {
            std::lock_guard lock(mutex_);
            if (!query_ || !push_ || !pop_ || !recorded_ || !event_)
                return {State::Unavailable, "event_or_exports_unavailable"};
            cuda_canvas::ContextScope scope(push_, pop_);
            const auto opened = scope.open(context_);
            if (opened != CUDA_SUCCESS)
                return {State::Error, "context_push", static_cast<int>(opened)};
            const auto queried = query_(event_);
            const auto restored = scope.close();
            const auto state = queried == CUDA_SUCCESS ? State::Ready :
                queried == CUDA_ERROR_NOT_READY ? State::Pending : State::Error;
            return {restored == CUDA_SUCCESS ? state : State::Error,
                restored == CUDA_SUCCESS ? "event_query" : "context_pop",
                static_cast<int>(queried), static_cast<int>(restored)};
        } catch (...) {
            return {State::Unavailable, "query_exception"};
        }
    }
    int complete() noexcept
    {
        try {
        std::lock_guard lock(mutex_);
        if (result_) return *result_;
        cuda_canvas::ContextScope scope(push_, pop_);
        auto code = scope.open(context_);
        if (code == CUDA_SUCCESS) {
            code = recorded_ ? synchronize_(event_) : drain_(producer_);
            const auto closed = scope.close();
            if (code == CUDA_SUCCESS) code = closed;
            else cuda_canvas::reportCleanup(closed, "readiness_completion_context_pop");
        }
        result_ = static_cast<int>(code);
        return *result_;
        } catch (...) {
            cuda_canvas::reportCleanup(CUDA_ERROR_UNKNOWN, "readiness_completion_lock_failure");
            return static_cast<int>(CUDA_ERROR_UNKNOWN);
        }
    }
    const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept override { return resource_; }
    ::media::Status enqueueCudaWait(void* context, void* stream) override
    {
        std::lock_guard lock(mutex_);
        if (result_ && *result_ != static_cast<int>(CUDA_SUCCESS))
            return cuda_canvas::status(static_cast<CUresult>(*result_), "CUDA readiness previous completion");
        if (!recorded_ || context != context_)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("CUDA readiness consumer context differs from producer"));
        cuda_canvas::ContextScope scope(push_, pop_);
        auto code = scope.open(context_);
        if (code != CUDA_SUCCESS) return cuda_canvas::status(code, "CUDA readiness wait context push");
        code = wait_(static_cast<CUstream>(stream), event_, 0);
        const auto restored = scope.close();
        if (code != CUDA_SUCCESS) {
            cuda_canvas::reportCleanup(restored, "readiness_wait_context_pop");
            return cuda_canvas::status(code, "CUDA readiness stream wait");
        }
        return cuda_canvas::status(restored, "CUDA readiness wait context pop");
    }
private:
    std::shared_ptr<MediaPreparationMemoryResource> resource_;
    cuda_canvas::Library library_;
    BufferRefPtr device_;
    std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs_;
    CUcontext context_;
    CUstream producer_;
    CUevent event_ = nullptr;
    bool recorded_ = false;
    std::mutex mutex_;
    std::optional<int> result_;
    tcuCtxPushCurrent_v2* push_ = nullptr;
    tcuCtxPopCurrent_v2* pop_ = nullptr;
    tcuEventCreate* create_ = nullptr;
    tcuEventRecord* record_ = nullptr;
    tcuEventQuery* query_ = nullptr;
    tcuStreamWaitEvent* wait_ = nullptr;
    tcuEventSynchronize* synchronize_ = nullptr;
    tcuEventDestroy_v2* destroy_ = nullptr;
    tcuStreamSynchronize* drain_ = nullptr;
};
}
void completeMediaCudaProducerForRelease(const MediaCudaFrameProducerFacts& producer) noexcept
{ drainUnattachedProducer(static_cast<CUcontext>(producer.context), static_cast<CUstream>(producer.stream)); }
::media::Result<MediaSharedAllocationGeometry> mediaCudaFrameReadinessAllocationGeometry()
{
    return probeMediaSharedAllocationGeometry<CudaFrameReadiness>(
        std::type_identity<MediaLineageAllocator<CudaFrameReadiness>>{});
}

::media::Result<MediaCudaFrameProducerFacts> inspectMediaCudaFrameProducer(
    const AVFrame& frame, MediaCudaFrameProducer authority)
{
    using Result = ::media::Result<MediaCudaFrameProducerFacts>;
    if (frame.format != AV_PIX_FMT_CUDA || !frame.hw_frames_ctx || !frame.hw_frames_ctx->data)
        return Result::failure(::media::ErrorInfo::unsupported("Producer readiness requires actual CUDA frame device"));
    const auto* frames = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    if (!frames->device_ctx || frames->device_ctx->type != AV_HWDEVICE_TYPE_CUDA || !frames->device_ctx->hwctx)
        return Result::failure(::media::ErrorInfo::unsupported("Producer CUDA device facts unavailable"));
    const auto* device = static_cast<const AVCUDADeviceContext*>(frames->device_ctx->hwctx);
    if (!device->cuda_ctx) return Result::failure(::media::ErrorInfo::notInitialized("Producer CUDA context unavailable"));
    return Result::success({authority, device->cuda_ctx, device->stream, std::nullopt});
}
::media::Result<MediaVideoFrameReadiness> captureMediaCudaFrameReadiness(
    AVFrame& frame, const std::shared_ptr<MediaPreparationMemoryResource>& resource,
    const MediaCudaFrameProducerFacts& producer,
    std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs)
{
    using Result = ::media::Result<MediaVideoFrameReadiness>;
    const bool known = producer.kind == MediaCudaFrameProducer::NativeSafeDecode ||
        producer.kind == MediaCudaFrameProducer::CuvidDecode || producer.kind == MediaCudaFrameProducer::ScaleCuda ||
        producer.kind == MediaCudaFrameProducer::HardwareTransfer;
    if (frame.format != AV_PIX_FMT_CUDA ||
        !frame.hw_frames_ctx || !frame.hw_frames_ctx->data)
        return Result::failure(::media::ErrorInfo::invalidArgument("CUDA readiness requires admitted producer and frame facts"));
    const auto* frames = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    if (!frames->device_ctx || !frames->device_ref || frames->device_ctx->type != AV_HWDEVICE_TYPE_CUDA || !frames->device_ctx->hwctx)
        return Result::failure(::media::ErrorInfo::invalidArgument("CUDA readiness frame device is absent"));
    const auto* hardware = static_cast<const AVCUDADeviceContext*>(frames->device_ctx->hwctx);
    if (!known || !resource || !producer.context || producer.context != hardware->cuda_ctx || producer.stream != hardware->stream) {
        drainUnattachedProducer(hardware->cuda_ctx, hardware->stream);
        return Result::failure(::media::ErrorInfo::invalidArgument("CUDA readiness frame differs from admitted producer stream"));
    }
    std::shared_ptr<CudaFrameReadiness> operation;
    if (auto valid = validateMediaVideoFrameReferenceContract(frame, producer); !valid) {
        drainUnattachedProducer(hardware->cuda_ctx, hardware->stream);
        return Result::failure(valid.error());
    }
    try {
        auto device = makeBufferRef(frames->device_ref);
        if (!device) {
            drainUnattachedProducer(hardware->cuda_ctx, hardware->stream);
            return Result::failure(::media::ErrorInfo::allocationFailed("CUDA readiness device reference"));
        }
        operation = std::allocate_shared<CudaFrameReadiness>(
            MediaLineageAllocator<CudaFrameReadiness>(MediaPreparedLineageAllocation{resource}),
            resource, std::move(device), hardware->cuda_ctx, hardware->stream, std::move(consumedInputs));
        auto recorded = operation->record();
        if (!recorded) return Result::failure(recorded.error());
        auto completion = MediaFfmpegPayloadCompletion::create(resource, operation,
            [](void* value) noexcept { return static_cast<CudaFrameReadiness*>(value)->complete(); }, CUDA_SUCCESS);
        if (!completion) return Result::failure(completion.error());
        auto bound = retainMediaFfmpegPayload(frame,
            {nullptr, completion.value(), MediaPreparedLineageAllocation{resource}});
        if (!bound) return Result::failure(bound.error());
        return Result::success(MediaVideoFrameReadiness(MediaVideoFrameReadiness::Kind::CudaEvent,
            std::move(operation), std::move(completion).value()));
    } catch (const std::bad_alloc&) {
        if (!operation) drainUnattachedProducer(hardware->cuda_ctx, hardware->stream);
        return Result::failure(::media::ErrorInfo::allocationFailed("CUDA frame readiness metadata"));
    }
}
} // namespace media::ffmpeg::graph
#else
namespace media::ffmpeg::graph {
void completeMediaCudaProducerForRelease(const MediaCudaFrameProducerFacts&) noexcept {}
::media::Result<MediaSharedAllocationGeometry> mediaCudaFrameReadinessAllocationGeometry()
{ return ::media::Result<MediaSharedAllocationGeometry>::failure(
    ::media::ErrorInfo::unsupported("CUDA readiness allocation geometry unavailable on this build")); }
::media::Result<MediaCudaFrameProducerFacts> inspectMediaCudaFrameProducer(const AVFrame&, MediaCudaFrameProducer)
{ return ::media::Result<MediaCudaFrameProducerFacts>::failure(::media::ErrorInfo::unsupported("CUDA producer readiness unavailable on this build")); }
::media::Result<MediaVideoFrameReadiness> captureMediaCudaFrameReadiness(
    AVFrame&, const std::shared_ptr<MediaPreparationMemoryResource>&, const MediaCudaFrameProducerFacts&,
    std::shared_ptr<const MediaFfmpegPayloadReferences>)
{ return ::media::Result<MediaVideoFrameReadiness>::failure(::media::ErrorInfo::unsupported("CUDA readiness adapter is unavailable on this build")); }
}
#endif
