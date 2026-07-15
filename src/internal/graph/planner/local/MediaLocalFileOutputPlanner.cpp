#include "internal/graph/planner/local/MediaLocalFileOutputPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaLocalFileOutputPlan> MediaLocalFileOutputPlanner::plan(
    std::string outputUrl,
    std::string outputFormat)
{
    if (outputUrl.empty()) {
        return ::media::Result<MediaLocalFileOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaLocalFileOutputPlanner requires output URL"));
    }
    MediaLocalFileOutputPlan plan;
    plan.url = std::move(outputUrl);
    plan.format = std::move(outputFormat);
    plan.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    return ::media::Result<MediaLocalFileOutputPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
