#include "internal/graph/planner/realtime/MediaVideoSharedSourcePlanner.h"
#include "internal/graph/planner/MediaVideoFilterExecutionPlanner.h"
#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"

namespace media::ffmpeg::graph {
::media::Result<MediaVideoSharedSourcePlan> MediaVideoSharedSourcePlanner::plan(
    const MediaPipelineChainPlan& chain, const MediaInputVideoStreamInfo& source)
{
    using Result = ::media::Result<MediaVideoSharedSourcePlan>;
    if (chain.decoder.deviceKind() != MediaHardwareDeviceKind::RKMPP)
        return Result::success({MediaVideoSourceAllocation::DecoderOutput, std::nullopt, MediaVideoFilterImplementation::None});
    auto evidence = MediaVideoSourceIsolationAdapter::inspect(chain.decoder.deviceKind());
    if (!evidence) return Result::failure(evidence.error());
    auto copy = MediaVideoFilterExecutionPlanner::sourceCopy(source.frameRate,
        source.sampleAspectRatio, std::move(evidence).value());
    if (!copy) return Result::failure(copy.error());
    return Result::success({MediaVideoSourceAllocation::IndependentFilterOutput, std::move(copy).value(), MediaVideoFilterImplementation::Rga});
}
} // namespace media::ffmpeg::graph
