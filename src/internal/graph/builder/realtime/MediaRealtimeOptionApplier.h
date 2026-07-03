#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeOptionApplier final {
public:
    static ::media::Result<void> applyInputOptions(MediaGraph& graph,
                                                   MediaNodeId nodeId,
                                                   const MediaRealtimeGraphBuilderOptions& options);
    static ::media::Result<void> applyOutputOptions(MediaGraph& graph,
                                                    MediaNodeId nodeId,
                                                    const MediaRealtimeGraphBuilderOptions& options);
    static ::media::Result<void> applySdpWriterOptions(MediaGraph& graph,
                                                       MediaNodeId nodeId,
                                                       const MediaRealtimeGraphBuilderOptions& options);

private:
    MediaRealtimeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
