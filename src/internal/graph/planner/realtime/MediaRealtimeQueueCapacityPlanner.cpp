#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"

#include <limits>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::size_t> representable(
    std::uint64_t value,
    const char* fact)
{
    if (value > static_cast<std::uint64_t>(
            (std::numeric_limits<std::size_t>::max)())) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Realtime deployment ") + fact +
                " exceeds platform queue capacity"));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(value));
}

} // namespace

::media::Result<MediaGraphQueueParameters> MediaRealtimeQueueCapacityPlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment)
{
    const auto& resources = deployment.encode().resources;
    auto backlog = representable(
        resources.maximumBacklogDatagrams, "maximum backlog datagrams");
    if (!backlog) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            backlog.error());
    }
    auto batch = representable(
        resources.maximumBatchDatagrams, "maximum batch datagrams");
    if (!batch) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            batch.error());
    }
    return ::media::Result<MediaGraphQueueParameters>::success(
        MediaGraphQueueParameters{1, backlog.value(), batch.value(),
                                  backlog.value()});
}

} // namespace media::ffmpeg::graph
