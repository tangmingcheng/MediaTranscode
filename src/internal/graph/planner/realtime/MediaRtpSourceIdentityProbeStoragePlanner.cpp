#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbeStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbe.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpSourceIdentityProbeStorage> MediaRtpSourceIdentityProbeStoragePlanner::plan(
    std::size_t maximumSubmissions, std::size_t additionalSoftwareFrameBytes)
{
    using R = ::media::Result<MediaRtpSourceIdentityProbeStorage>;
    using Resource = MediaPreparationMemoryResource;
    using Budget = MediaPreparationStorageBudget;
    auto registry = MediaCodecLineageRegistry::preparedStorage(maximumSubmissions);
    auto resourceGeometry = probeMediaSharedAllocationGeometry<Resource>(std::type_identity<Resource::BootstrapAllocator>{});
    auto probeGeometry = probeMediaSharedAllocationGeometry<MediaRtpSourceIdentityProbe>(
        std::type_identity<MediaLineageAllocator<MediaRtpSourceIdentityProbe>>{});
    auto budgetGeometry = probeMediaSharedAllocationGeometry<Budget>(std::type_identity<Budget::BootstrapAllocator>{});
    if (!registry) return R::failure(registry.error());
    if (!resourceGeometry) return R::failure(resourceGeometry.error());
    if (!probeGeometry) return R::failure(probeGeometry.error());
    if (!budgetGeometry) return R::failure(budgetGeometry.error());
    auto bootstrap = Resource::bootstrapAllocationFootprint(resourceGeometry.value().bytes, resourceGeometry.value().alignment);
    auto probe = Resource::allocationFootprint(probeGeometry.value().bytes, probeGeometry.value().alignment);
    if (!bootstrap) return R::failure(bootstrap.error());
    if (!probe) return R::failure(probe.error());
    // Both bootstrap paths use a block owned by the original preparation budget.
    auto budget = Resource::bootstrapAllocationFootprint(budgetGeometry.value().bytes, budgetGeometry.value().alignment);
    if (!budget) return R::failure(budget.error());
    // Original opaque reference per submitted AU, one cloned packet and its
    // payload reference, one receive frame beside the retained first frame.
    auto headers = MediaCheckedArithmetic::multiply(maximumSubmissions, sizeof(AVBufferRef), "probe public references");
    for (const auto bytes : {sizeof(AVPacket), sizeof(AVBufferRef), sizeof(AVFrame), additionalSoftwareFrameBytes}) {
        if (!headers) return R::failure(headers.error());
        headers = MediaCheckedArithmetic::add(headers.value(), bytes, "probe public headers");
    }
    if (!headers) return R::failure(headers.error());
    auto total = MediaCheckedArithmetic::add(registry.value().totalBytes, headers.value(), "probe storage");
    for (const auto bytes : {bootstrap.value(), probe.value()}) {
        if (!total) return R::failure(total.error());
        total = MediaCheckedArithmetic::add(total.value(), bytes, "probe storage");
    }
    if (!total) return R::failure(total.error());
    const auto capacity = total.value();
    total = MediaCheckedArithmetic::add(capacity, budget.value(), "probe root storage");
    if (!total) return R::failure(total.error());
    if (total.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Probe storage exceeds address space"));
    return R::success({registry.value(), bootstrap.value(), probe.value(),
        static_cast<std::size_t>(headers.value()), static_cast<std::size_t>(capacity),
        budget.value(), static_cast<std::size_t>(total.value())});
}

} // namespace media::ffmpeg::graph
