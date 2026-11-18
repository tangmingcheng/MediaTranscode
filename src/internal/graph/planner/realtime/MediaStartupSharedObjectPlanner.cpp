#include "internal/graph/planner/realtime/MediaStartupSharedObjectPlanner.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaSharedAllocationGeometry> geometry(MediaStartupSharedObjectRole role)
{
    switch (role) {
    case MediaStartupSharedObjectRole::Envelope:
        return probeMediaSharedAllocationGeometry<MediaAvStartupEnvelopeBuffer>(
            std::type_identity<MediaAvStartupEnvelopeBuffer::AllocationAllocator>{});
    case MediaStartupSharedObjectRole::Release:
        return probeMediaSharedAllocationGeometry<MediaAvStartupReleaseBuffer>(
            std::type_identity<MediaAvStartupReleaseBuffer::AllocationAllocator>{});
    case MediaStartupSharedObjectRole::ReleaseTransaction:
    case MediaStartupSharedObjectRole::ControlTransaction:
        return probeMediaSharedAllocationGeometry<MediaStartupReleaseTransactionBuffer>(
            std::type_identity<MediaStartupReleaseTransactionBuffer::AllocationAllocator>{});
    }
    return ::media::Result<MediaSharedAllocationGeometry>::failure(
        ::media::ErrorInfo::invalidArgument("Unknown startup shared object role"));
}
}
::media::Result<MediaPreparedStartupSharedObject> MediaStartupSharedObjectPlanner::prepared(MediaStartupSharedObjectRole role)
{
    using R = ::media::Result<MediaPreparedStartupSharedObject>;
    auto request = geometry(role);
    if (!request) return R::failure(request.error());
    auto bytes = MediaPreparationMemoryResource::allocationFootprint(request.value().bytes, request.value().alignment);
    if (!bytes) return R::failure(bytes.error());
    return R::success({request.value(), bytes.value()});
}
::media::Result<MediaStartupSharedObjectPlan> MediaStartupSharedObjectPlanner::plan(
    MediaStartupSharedObjectRole role, MediaLineageAllocationScope allocation)
{
    using R = ::media::Result<MediaStartupSharedObjectPlan>;
    switch (role) {
    case MediaStartupSharedObjectRole::Envelope:
    case MediaStartupSharedObjectRole::Release:
    case MediaStartupSharedObjectRole::ReleaseTransaction:
    case MediaStartupSharedObjectRole::ControlTransaction: break;
    default: return R::failure(::media::ErrorInfo::invalidArgument("Unknown startup shared object role"));
    }
    MediaStartupSharedObjectPlan result{role, std::move(allocation), std::nullopt};
    if (std::holds_alternative<MediaRuntimeLineageAllocation>(result.m_allocation))
        return R::success(std::move(result));
    if (!std::get<MediaPreparedLineageAllocation>(result.m_allocation).resource)
        return R::failure(::media::ErrorInfo::invalidArgument("Startup shared object has no prepared owner"));
    auto storage = prepared(role);
    if (!storage) return R::failure(storage.error());
    result.m_prepared = storage.value();
    return R::success(std::move(result));
}
::media::Status MediaStartupSharedObjectPlanner::validate(
    const MediaStartupSharedObjectPlan& plan, MediaStartupSharedObjectRole role)
{
    const bool prepared = std::holds_alternative<MediaPreparedLineageAllocation>(plan.allocation());
    if (plan.role() != role || prepared != plan.prepared().has_value() ||
        !MediaLineageAllocator<std::byte>{plan.allocation()}.resource())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup shared object differs from its planned role or owner"));
    if (plan.prepared()) {
        const auto& request = plan.prepared()->geometry;
        auto bytes = MediaPreparationMemoryResource::allocationFootprint(request.bytes, request.alignment);
        if (request.count != 1 || !request.bytes || !bytes || bytes.value() != plan.prepared()->allocationBytes)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Startup shared object footprint differs from its planned request"));
    }
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
