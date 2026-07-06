#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeRtpTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const MediaRealtimeGraphBuilderOptions& options);
    static ::media::Status validate(const MediaRealtimeGraphBuilderOptions& options);

private:
    MediaRealtimeRtpTranscodeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
