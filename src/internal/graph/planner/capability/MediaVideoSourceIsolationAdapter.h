#pragma once
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"
struct AVFilterGraph;

namespace media::ffmpeg::graph {
class MediaVideoSourceIsolationAdapter final {
public:
    static ::media::Result<MediaVideoFilterIsolationEvidence> inspect(MediaHardwareDeviceKind device);
    static ::media::Result<MediaVideoFilterIsolationEvidence> inspect(
        const AVFilterGraph& graph, std::string description);
};
} // namespace media::ffmpeg::graph
