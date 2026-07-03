#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeIngestToMuxGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const MediaRealtimeGraphBuilderOptions& options);

private:
    MediaRealtimeIngestToMuxGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
