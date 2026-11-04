#include "internal/graph/nodes/video/MediaVideoFrameReadinessBinding.h"
#include "internal/graph/nodes/video/MediaVideoFrameReadinessPlanCodec.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
namespace media::ffmpeg::graph {
::media::Result<MediaVideoProducerFailureScope> MediaVideoFrameReadinessBinding::guard(const MediaNodeOptions* options)
{
    using Result = ::media::Result<MediaVideoProducerFailureScope>;
    auto plan = options ? MediaVideoFrameReadinessPlanCodec::decode(*options)
        : ::media::Result<std::optional<MediaVideoFrameProducerFacts>>::success(std::nullopt);
    if (!plan) return Result::failure(plan.error());
    return Result::success(MediaVideoProducerFailureScope(std::move(plan).value()));
}

::media::Result<MediaBufferRef> MediaVideoFrameReadinessBinding::publish(::media::ffmpeg::FramePtr frame,
    const MediaNodeOptions* options, const std::shared_ptr<MediaPreparationMemoryResource>& resource,
        std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs)
{
    using Result = ::media::Result<MediaBufferRef>;
    auto plan = options ? MediaVideoFrameReadinessPlanCodec::decode(*options)
        : ::media::Result<std::optional<MediaVideoFrameProducerFacts>>::success(std::nullopt);
    if (!plan) return Result::failure(plan.error());
    MediaVideoProducerFailureScope failure(plan.value());
    if (!plan.value()) return FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video, MediaRuntimeLineageAllocation{});
    if (!frame || !resource) return Result::failure(::media::ErrorInfo::notInitialized("Planned readiness requires frame and reference budget"));
    auto ready = captureMediaVideoFrameReadiness(*frame, resource, *plan.value(), std::move(consumedInputs));
    if (!ready) return Result::failure(ready.error());
    failure.release();
    auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Video, MediaPreparedLineageAllocation{resource});
    if (!wrapped) return wrapped;
    auto* target = dynamic_cast<FFmpegFrameBuffer*>(wrapped.value().get());
    if (!target) return Result::failure(::media::ErrorInfo::internalError("Readiness frame wrapper is absent"));
    auto bound = target->setReadiness(std::move(ready).value());
    if (!bound) return Result::failure(bound.error());
    return wrapped;
}
::media::Status MediaVideoFrameReadinessBinding::admitInput(const MediaBufferRef& buffer, const MediaNodeOptions* options)
{
    auto plan = options ? MediaVideoFrameReadinessPlanCodec::decode(*options)
        : ::media::Result<std::optional<MediaVideoFrameProducerFacts>>::success(std::nullopt);
    if (!plan) return ::media::Status::failure(plan.error());
    if (!plan.value()) return ::media::Status::success();
    const auto* frame = FFmpegFrameView::frame(buffer);
    if (!frame) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Readiness input frame missing"));
    // The upstream publisher validated its own reference bound. This node's
    // producer describes its output, which may have a different allocation.
    const auto* ready = FFmpegFrameView::readiness(buffer);
    if (!ready) return ::media::Status::failure(::media::ErrorInfo::notInitialized("Planned filter input readiness missing"));
    if (const auto* cuda = std::get_if<MediaCudaFrameProducerFacts>(&*plan.value()))
        return ready->enqueueCudaWait(cuda->context, cuda->stream);
    // A synchronous consumer must finish its input dependency before entering
    // the platform API; unlike CUDA it cannot enqueue a foreign stream wait.
    return ready->ensureCompleted();
}
}
