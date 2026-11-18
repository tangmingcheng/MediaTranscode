#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsResolvedPipelineFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAvOutputSynchronizationRequest final {
    const std::string& mediaId;
    std::optional<RealtimeOutputStreamLayout> layout;
    std::optional<MediaOutputTransportKind> transport;
    const MediaFrameRateParameters& videoFrameRate;
    int audioSampleRate;
    const MediaRealtimeDeploymentEnvelope& deployment;
    const MediaProjectMpegTsResolvedPipelineFacts* mpegTsFacts;
};

class MediaAvOutputSynchronizationPlanner final {
public:
    static ::media::Result<MediaAvSyncPlan> plan(
        const MediaAvOutputSynchronizationRequest& request);
private:
    MediaAvOutputSynchronizationPlanner() = delete;
};

} // namespace media::ffmpeg::graph
