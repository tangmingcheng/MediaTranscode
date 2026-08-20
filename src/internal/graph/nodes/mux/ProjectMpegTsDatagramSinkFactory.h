#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;
class MediaScheduledDatagramBatchBuilder;

struct ProjectMpegTsDatagramBinding final {
    std::unique_ptr<MediaTsDatagramSink> sink;
    std::shared_ptr<MediaScheduledDatagramBatchBuilder> scheduledBatch;
};

class ProjectMpegTsDatagramSinkFactory final {
public:
    static ::media::Result<bool> bindingsReady(
        const MediaTsMuxPlan& muxPlan,
        const MediaOutputByteSink* udpByteSink);
    static ::media::Result<ProjectMpegTsDatagramBinding> create(
        const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
        const MediaTsMuxPlan& muxPlan,
        const MediaProtocolOutputActivation& activation,
        MediaOutputByteSink* udpByteSink);

private:
    ProjectMpegTsDatagramSinkFactory() = delete;
};

} // namespace media::ffmpeg::graph
