#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpSdpSessionIdentityMaterializer final {
public:
    static ::media::Result<MediaSdpSessionIdentity> materialize(
        const MediaSeparateRtpSdpRuntimePlan& plan,
        const MediaSharedNtpEpoch& sharedNtpEpoch,
        std::uint64_t activePlaybackGeneration);

private:
    MediaRtpSdpSessionIdentityMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
