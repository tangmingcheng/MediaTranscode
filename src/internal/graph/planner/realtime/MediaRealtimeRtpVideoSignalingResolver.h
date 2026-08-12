#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaRealtimeRtpVideoSignalingResolver final {
public:
    static ::media::Result<std::string> resolveFmtp(
        const MediaRealtimeRtpInputMetadata& requested,
        const MediaDetectedRtpVideoSignaling* detected);

private:
    MediaRealtimeRtpVideoSignalingResolver() = delete;
};

} // namespace media::ffmpeg::graph
