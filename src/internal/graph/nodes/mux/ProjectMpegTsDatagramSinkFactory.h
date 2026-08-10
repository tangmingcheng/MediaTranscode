#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpContinuityState.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;
class ProjectMpegTsDatagramSinkFactory final {
public:
    static ::media::Result<bool> bindingsReady(
        const MediaTsMuxPlan& muxPlan,
        const std::shared_ptr<const MediaSharedNtpEpoch>& sharedNtpEpoch,
        const MediaOutputByteSink* udpByteSink);
    static ::media::Result<std::unique_ptr<MediaTsDatagramSink>> create(
        const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
        const MediaTsMuxPlan& muxPlan,
        const MediaProtocolOutputActivation& activation,
        const std::shared_ptr<const MediaSharedNtpEpoch>& sharedNtpEpoch,
        const std::shared_ptr<MediaMpegTsRtpContinuityState>&
            rtpContinuity,
        MediaOutputByteSink* udpByteSink);

private:
    ProjectMpegTsDatagramSinkFactory() = delete;
};

} // namespace media::ffmpeg::graph
