#include "internal/graph/runtime/buffer/MediaDatagramShapingPlanBuffer.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramShapingPlanBuffer::MediaDatagramShapingPlanBuffer(
    MediaDatagramShapingPlan plan) noexcept
    : m_plan(std::move(plan))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::DatagramShapingPlan);
    setDiagnosticName("datagram_shaping_plan");
}

::media::Result<std::shared_ptr<MediaDatagramShapingPlanBuffer>>
MediaDatagramShapingPlanBuffer::create(MediaDatagramShapingPlan plan)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaDatagramShapingPlanBuffer>>;
    try {
        return Result::success(
            std::shared_ptr<MediaDatagramShapingPlanBuffer>(
                new MediaDatagramShapingPlanBuffer(std::move(plan))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramShapingPlanBuffer"));
    }
}

::media::Result<std::shared_ptr<MediaDatagramShapingPlanBuffer>>
MediaDatagramShapingPlanBuffer::clone() const
{
    auto planClone = m_plan.clone();
    if (!planClone) {
        return ::media::Result<
            std::shared_ptr<MediaDatagramShapingPlanBuffer>>::failure(
                planClone.error());
    }
    return create(std::move(planClone).value());
}

} // namespace media::ffmpeg::graph
