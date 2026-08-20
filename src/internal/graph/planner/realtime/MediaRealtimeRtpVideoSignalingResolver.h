#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaResolvedRtpVideoSignaling final {
    std::string fmtp;
    MediaSize codedSize;
};

class MediaRealtimeRtpVideoSignalingResolver final {
public:
    static ::media::Result<MediaResolvedRtpVideoSignaling> resolve(
        const MediaRealtimeRtpInputMetadata& requested,
        const MediaDetectedRtpVideoSignaling* detected);

private:
    MediaRealtimeRtpVideoSignalingResolver() = delete;
};

} // namespace media::ffmpeg::graph
