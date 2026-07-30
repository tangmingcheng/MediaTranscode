#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;
class MediaUdpDatagramSenderPortFactory;

class ProjectMpegTsDatagramSinkFactory final {
public:
    static ::media::Result<std::unique_ptr<MediaTsDatagramSink>> create(
        const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
        const MediaPlaybackEpoch& epoch,
        const MediaSharedNtpEpoch* sharedNtpEpoch,
        MediaUdpDatagramSenderPortFactory& datagramPortFactory,
        MediaOutputByteSink* udpByteSink);

private:
    ProjectMpegTsDatagramSinkFactory() = delete;
};

} // namespace media::ffmpeg::graph
