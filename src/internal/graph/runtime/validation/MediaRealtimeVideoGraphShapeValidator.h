#pragma once

#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraph;
struct MediaRealtimeVideoRuntimeBinding;

class MediaRealtimeVideoGraphShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaRealtimeVideoRuntimeBinding& binding);
    static ::media::Status validateAbsent(const MediaGraph& graph);

private:
    MediaRealtimeVideoGraphShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
