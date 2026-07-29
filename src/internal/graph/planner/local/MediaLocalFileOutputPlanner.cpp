#include "internal/graph/planner/local/MediaLocalFileOutputPlanner.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaLocalFileOutputPlan> MediaLocalFileOutputPlanner::plan(
    std::string outputUrl)
{
    if (outputUrl.empty()) {
        return ::media::Result<MediaLocalFileOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaLocalFileOutputPlanner requires output URL"));
    }
    const AVOutputFormat* outputFormat = av_guess_format(
        nullptr, outputUrl.c_str(), nullptr);
    if (!outputFormat || !outputFormat->name) {
        return ::media::Result<MediaLocalFileOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaLocalFileOutputPlanner cannot resolve muxer from output URL"));
    }
    MediaLocalFileOutputPlan plan;
    plan.url = std::move(outputUrl);
    plan.format = outputFormat->name;
    plan.outputResourceKind = MediaOutputResourceKind::FFmpegFormatContext;
    plan.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    return ::media::Result<MediaLocalFileOutputPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
