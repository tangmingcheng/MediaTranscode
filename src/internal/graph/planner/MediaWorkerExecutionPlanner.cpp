#include "internal/graph/planner/MediaWorkerExecutionPlanner.h"
#include "internal/graph/runtime/threading/MediaWorkerThreadRegistration.h"
#include <algorithm>
#include <new>
namespace media::ffmpeg::graph {
::media::Result<MediaFixedCapacityStorage<MediaWorkerExecutionPlan>> MediaWorkerExecutionPlanner::plan(
    const MediaGraph& graph, std::span<const MediaNodeId> nodes) try {
    using Result = ::media::Result<MediaFixedCapacityStorage<MediaWorkerExecutionPlan>>;
    MediaFixedCapacityStorage<MediaWorkerExecutionPlan> result(nodes.size());
    bool distribution = false;
    for (const auto id : nodes) {
        const auto* node = graph.findNode(id);
        if (!node || std::count(nodes.begin(), nodes.end(), id) != 1)
            return Result::failure(::media::ErrorInfo::invalidArgument("Worker planning requires exact unique graph nodes"));
        const bool sender = node->kind == MediaNodeKind::ScheduledDatagramSender;
        distribution |= sender;
        result.push_back({id, sender ? MediaWorkerExecutionRole::DatagramDistribution : MediaWorkerExecutionRole::Ordinary,
#ifdef _WIN32
            sender ? MediaWorkerPlatformExecution::WindowsDistribution : MediaWorkerPlatformExecution::WindowsOrdinary
#else
            MediaWorkerPlatformExecution::LinuxNative
#endif
        });
    }
#ifdef _WIN32
    if (distribution) {
        auto capability = probeMediaWindowsDistributionRegistration();
        if (!capability) return Result::failure(capability.error());
    }
#else
    (void)distribution;
#endif
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaFixedCapacityStorage<MediaWorkerExecutionPlan>>::failure(::media::ErrorInfo::allocationFailed("worker execution contracts"));
}
}
