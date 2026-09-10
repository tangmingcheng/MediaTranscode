#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoOutputRequest.h"
#include "media_transcode/Result.h"
#include "media_transcode_beta/realtime.h"

namespace media::beta {

class MediaRealtimeBetaRequestMapper final {
public:
    MediaRealtimeBetaRequestMapper() = delete;
    static ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest> map(
        const mt_beta_realtime_config& config);
    static ::media::Result<ffmpeg::graph::MediaRealtimeVideoOutputRequest> mapOutput(
        const mt_beta_video_output& output);
};

} // namespace media::beta
