#include "internal/graph/planner/realtime/MediaRealtimeVideoParametersPlanner.h"
#include <utility>
namespace media::ffmpeg::graph {
constexpr int RealtimeNoBidirectionalFrames = 0;

::media::Result<MediaVideoTranscodeParameters> MediaRealtimeVideoParametersPlanner::plan(
    const MediaRealtimeVideoTranscodeParameters& requested,
    std::optional<RealtimeOutputStreamLayout> outputLayout)
{
    using Result = ::media::Result<MediaVideoTranscodeParameters>;
    if (!outputLayout || (*outputLayout != RealtimeOutputStreamLayout::SeparateStreams &&
        *outputLayout != RealtimeOutputStreamLayout::MuxedTransportStream))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Realtime encoder parameters require a supported explicit output layout"));
    MediaVideoTranscodeParameters planned;
    planned.codecName = requested.codecName;
    planned.profile = requested.profile;
    planned.width = requested.width;
    planned.height = requested.height;
    planned.frameRate = requested.frameRate;
    planned.rateControl = requested.rateControl;
    planned.bitrateKbps = requested.bitrateKbps;
    planned.minBitrateKbps = requested.minBitrateKbps;
    planned.maxBitrateKbps = requested.maxBitrateKbps;
    planned.gop = requested.gop;
    planned.bFrames = RealtimeNoBidirectionalFrames;
    if (*outputLayout == RealtimeOutputStreamLayout::MuxedTransportStream)
        planned.globalHeader = true;
    return Result::success(std::move(planned));
}

} // namespace media::ffmpeg::graph
