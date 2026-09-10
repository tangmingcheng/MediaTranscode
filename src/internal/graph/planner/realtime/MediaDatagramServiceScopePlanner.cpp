#include "internal/graph/planner/realtime/MediaDatagramServiceScopePlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaDatagramServiceScopeContract> MediaDatagramServiceScopePlanner::plan(
    const MediaDatagramTransportPlanTemplate& transport)
{
    using Result = ::media::Result<MediaDatagramServiceScopeContract>;
    const auto& facts = transport.encode().deployment;
    const auto& scope = facts.serviceScope;
    const auto& service = facts.service;
    if ((scope.kind != MediaDatagramServiceScopeKind::ManagedEgress &&
         scope.kind != MediaDatagramServiceScopeKind::ProvisionedEgress) ||
        scope.scopeId.empty() || scope.coverageAuthority.empty() ||
        service.authority.empty() || service.provisionedCapacityWireBytesPerSecond == 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Shared datagram service requires authoritative interface scope and capacity"));
    }
    return Result::success({scope.kind, scope.scopeId, scope.coverageAuthority,
        service.provisionedCapacityWireBytesPerSecond, service.authority});
}

} // namespace media::ffmpeg::graph
