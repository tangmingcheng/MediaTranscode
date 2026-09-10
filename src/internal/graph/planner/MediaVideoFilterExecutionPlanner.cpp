#include "internal/graph/planner/MediaVideoFilterExecutionPlanner.h"
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoFilterExecutionPlan> MediaVideoFilterExecutionPlanner::forEncoder(
    std::string filterDescription)
{
    MediaVideoFilterExecutionPlan plan{MediaVideoFilterTimingAuthority::PreparedEncoderMetadata,
        MediaVideoFilterAspectPolicy::PreserveSourceAspect,
        {std::move(filterDescription), MediaVideoFilterAllocation::NegotiatedOutput,
         MediaVideoFilterCompletion::NegotiatedOutput, {}, {}, 0, {}}, {}, {}};
    if (!plan.valid()) return ::media::Result<MediaVideoFilterExecutionPlan>::failure(
        ::media::ErrorInfo::invalidArgument("encoder filter execution requires its planned filter description"));
    return ::media::Result<MediaVideoFilterExecutionPlan>::success(std::move(plan));
}

::media::Result<MediaVideoFilterExecutionPlan> MediaVideoFilterExecutionPlanner::sourceCopy(
    MediaRational sourceFrameRate, MediaRational sourceSampleAspectRatio,
    MediaVideoFilterIsolationEvidence evidence)
{
    MediaVideoFilterExecutionPlan plan{MediaVideoFilterTimingAuthority::SourceFrame,
        MediaVideoFilterAspectPolicy::PreserveSourceAspect,
        std::move(evidence), sourceFrameRate, sourceSampleAspectRatio};
    if (!plan.valid()) return ::media::Result<MediaVideoFilterExecutionPlan>::failure(
        ::media::ErrorInfo::invalidArgument(
            "shared source filter requires authoritative timing, independent allocation and synchronous completion"));
    return ::media::Result<MediaVideoFilterExecutionPlan>::success(std::move(plan));
}
} // namespace media::ffmpeg::graph
