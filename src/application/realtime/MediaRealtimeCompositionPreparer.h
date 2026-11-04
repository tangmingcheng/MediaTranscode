#pragma once

#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionPrepared final {
    MediaRealtimeExecutableGraph executable;
    MediaThreadingPolicy threading;
    MediaRealtimeVideoPreparedReport report;
};

class MediaRealtimeCompositionPreparer final {
public:
    static ::media::Result<MediaRealtimeCompositionPrepared> prepare(
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaRealtimeVideoRunControl& control);
};

} // namespace media::ffmpeg::graph
