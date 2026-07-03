#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimePacketRelayGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const MediaRealtimeGraphBuilderOptions& options);

private:
    MediaRealtimePacketRelayGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
