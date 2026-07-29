#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"

#include <algorithm>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t MaximumRealtimePacketQueueCapacity = 256;
constexpr std::size_t MaximumRealtimeFrameQueueCapacity = 8;
constexpr std::size_t MaximumRealtimeMuxQueueCapacity = 32;

} // namespace

MediaGraphQueueParameters MediaRealtimeQueueCapacityPlanner::plan(
    const MediaGraphQueueParameters& requested)
{
    return {
        requested.metadata,
        std::min(requested.packet, MaximumRealtimePacketQueueCapacity),
        std::min(requested.frame, MaximumRealtimeFrameQueueCapacity),
        std::min(requested.mux, MaximumRealtimeMuxQueueCapacity)};
}

} // namespace media::ffmpeg::graph
