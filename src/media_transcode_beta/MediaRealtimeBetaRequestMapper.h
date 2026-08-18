#pragma once

#include "media_transcode/Result.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

#include <string>

namespace media::beta {

class MediaRealtimeBetaOwnedConfig;

class MediaRealtimeBetaRequestMapper final {
public:
    MediaRealtimeBetaRequestMapper() = delete;

    static ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest> map(
        const MediaRealtimeBetaOwnedConfig& config,
        const std::string& sessionOwnedSdpPath);
};

} // namespace media::beta
