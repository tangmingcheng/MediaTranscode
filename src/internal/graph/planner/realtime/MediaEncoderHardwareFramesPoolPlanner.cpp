#include "internal/graph/planner/realtime/MediaEncoderHardwareFramesPoolPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaEncoderHardwareFramesPoolPlan> MediaEncoderHardwareFramesPoolPlanner::plan(
    MediaEncoderSurfaceRetentionFacts facts)
{
    using Result = ::media::Result<MediaEncoderHardwareFramesPoolPlan>;
    if (facts.authority.empty()) return Result::failure(::media::ErrorInfo::notInitialized(
        "Encoder hardware pool requires retention authority"));
    auto inFlight = MediaCheckedArithmetic::add(facts.graphInFlightSurfaces,
        facts.pipelinePendingSurfaces, "graph in-flight and pending hardware surfaces");
    auto retained = inFlight ? MediaCheckedArithmetic::add(inFlight.value(),
        facts.encoderRetainedSurfaces, "encoder retained hardware surfaces") : inFlight;
    auto total = retained ? MediaCheckedArithmetic::add(retained.value(),
        facts.persistentPoolSurfaces, "persistent and circulating hardware surfaces") : retained;
    if (!total || total.value() == 0) return Result::failure(!total ? total.error() :
        ::media::ErrorInfo::notInitialized("Encoder hardware frame pool is empty"));
    return Result::success({std::move(facts), total.value()});
}
} // namespace media::ffmpeg::graph
