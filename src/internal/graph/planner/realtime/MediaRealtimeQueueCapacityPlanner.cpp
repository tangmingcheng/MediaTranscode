#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t MaximumRealtimeMetadataQueueCapacity = 1;
constexpr std::size_t MaximumRealtimePacketQueueCapacity = 256;
constexpr std::size_t MaximumRealtimeFrameQueueCapacity = 256;
constexpr std::size_t MaximumRealtimeMuxQueueCapacity = 256;

static_assert(MaximumRealtimeMetadataQueueCapacity <
              std::numeric_limits<std::size_t>::max());
static_assert(MaximumRealtimePacketQueueCapacity <
              std::numeric_limits<std::size_t>::max());
static_assert(MaximumRealtimeFrameQueueCapacity <
              std::numeric_limits<std::size_t>::max());
static_assert(MaximumRealtimeMuxQueueCapacity <
              std::numeric_limits<std::size_t>::max());

::media::Status validateMaximum(
    const char* queueName,
    std::size_t requested,
    std::size_t maximum)
{
    if (requested <= maximum) {
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("Realtime ") + queueName +
        " queue capacity exceeds the planner policy ceiling: requested=" +
        std::to_string(requested) + " maximum=" + std::to_string(maximum)));
}

} // namespace

::media::Result<MediaGraphQueueParameters> MediaRealtimeQueueCapacityPlanner::plan(
    const MediaGraphQueueParameters& requested)
{
    if (auto status = validateMaximum(
            "metadata", requested.metadata,
            MaximumRealtimeMetadataQueueCapacity); !status) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            status.error());
    }
    if (auto status = validateMaximum(
            "packet", requested.packet,
            MaximumRealtimePacketQueueCapacity); !status) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            status.error());
    }
    if (auto status = validateMaximum(
            "frame", requested.frame,
            MaximumRealtimeFrameQueueCapacity); !status) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            status.error());
    }
    if (auto status = validateMaximum(
            "mux", requested.mux,
            MaximumRealtimeMuxQueueCapacity); !status) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            status.error());
    }
    return ::media::Result<MediaGraphQueueParameters>::success(requested);
}

} // namespace media::ffmpeg::graph
