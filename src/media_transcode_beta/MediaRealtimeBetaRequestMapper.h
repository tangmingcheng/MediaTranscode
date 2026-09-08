#pragma once

#include "media_transcode/Result.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

namespace media::beta {

class MediaRealtimeBetaOwnedConfig;
class MediaRealtimeBetaTemporaryDescription;

class MediaRealtimeBetaRequestMapper final {
public:
    MediaRealtimeBetaRequestMapper() = delete;

    static ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest> map(
        const MediaRealtimeBetaOwnedConfig& config,
        const MediaRealtimeBetaTemporaryDescription& outputDescription);
};

} // namespace media::beta
