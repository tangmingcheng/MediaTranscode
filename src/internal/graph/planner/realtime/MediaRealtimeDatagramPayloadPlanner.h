#pragma once

#include "internal/graph/planner/realtime/MediaDatagramRouteProbe.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeDatagramPayloadPlanner final {
public:
    static ::media::Result<MediaRealtimeDeploymentMtuFact> plan(
        const MediaDatagramRouteFact& route);

private:
    MediaRealtimeDatagramPayloadPlanner() = delete;
};

} // namespace media::ffmpeg::graph
