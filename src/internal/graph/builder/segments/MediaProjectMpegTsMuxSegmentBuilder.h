#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaProjectMpegTsMuxSegmentOptions final {
    std::string prefix;
    bool expectVideo;
    bool expectAudio;
    MediaMuxSessionKind sessionKind;
    bool requireByteSinkResource;
};

class MediaProjectMpegTsMuxSegmentBuilder final {
public:
    static ::media::Result<MediaNodeId> build(
        MediaGraph& graph,
        const MediaProjectMpegTsMuxSegmentOptions& options);

private:
    MediaProjectMpegTsMuxSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
