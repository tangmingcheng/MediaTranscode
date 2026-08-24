#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeMediaCapacityPlan>
MediaRealtimeMediaCapacityPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.deployment ||
        !request.parameters.execution.streamSet ||
        request.parameters.queues.packet == 0) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "Realtime media capacity requires deployment, stream-set, and planned queue facts"));
    }
    const auto streamCount =
        *request.parameters.execution.streamSet ==
                MediaTranscodeStreamSet::AudioVideo
            ? std::uint64_t{2}
            : std::uint64_t{1};
    const auto& deployment = request.deployment->encode();
    const auto bytesPerStream =
        deployment.resources.maximumBacklogBytes / streamCount;
    if (bytesPerStream == 0 ||
        bytesPerStream > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime deployment byte budget cannot admit one media stream"));
    }
    MediaRealtimeMediaCapacityPlan product{
        request.parameters.queues.packet,
        bytesPerStream,
        bytesPerStream,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        deployment.latency.maximumResidence};
    if (streamCount == 2) {
        product.audioUnits = request.parameters.queues.packet;
        product.audioUnitBytes = bytesPerStream;
        product.audioBytes = bytesPerStream;
    }
    return ::media::Result<MediaRealtimeMediaCapacityPlan>::success(
        std::move(product));
}

} // namespace media::ffmpeg::graph
