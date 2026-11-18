#include "internal/graph/planner/MediaVideoFilterExecutionPlanner.h"
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoFilterExecutionPlan> MediaVideoFilterExecutionPlanner::bindOutputEvidence(
    MediaVideoFilterExecutionPlan plan, MediaVideoFilterIsolationEvidence evidence)
{
    using Result = ::media::Result<MediaVideoFilterExecutionPlan>;
    if (!plan.valid() || plan.output.filterDescription != evidence.filterDescription)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Filter output evidence differs from the planned implementation"));
    plan.output = std::move(evidence);
    if (!plan.valid()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Filter output evidence does not establish a complete execution contract"));
    return Result::success(std::move(plan));
}

::media::Result<MediaVideoFilterExecutionPlan> MediaVideoFilterExecutionPlanner::forNegotiatedOutput(
    std::string filterDescription, MediaVideoFrameLineageProduct frameProduct)
{
    MediaVideoFilterExecutionPlan plan{MediaVideoFilterTimingAuthority::PreparedEncoderMetadata, frameProduct,
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
    MediaVideoFilterExecutionPlan plan{MediaVideoFilterTimingAuthority::SourceFrame, MediaVideoFrameLineageProduct::CanonicalInterval,
        MediaVideoFilterAspectPolicy::PreserveSourceAspect,
        std::move(evidence), sourceFrameRate, sourceSampleAspectRatio};
    if (!plan.valid()) return ::media::Result<MediaVideoFilterExecutionPlan>::failure(
        ::media::ErrorInfo::invalidArgument(
            "shared source filter requires authoritative timing, independent allocation and synchronous completion"));
    return ::media::Result<MediaVideoFilterExecutionPlan>::success(std::move(plan));
}
} // namespace media::ffmpeg::graph
