#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeGraphBuilderResult> MediaRealtimeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return ::media::Result<MediaRealtimeGraphBuilderResult>::failure(graphResult.error());
    }

    MediaRealtimeGraphBuilderResult result;
    result.graph = std::move(graphResult).value();
    return ::media::Result<MediaRealtimeGraphBuilderResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
