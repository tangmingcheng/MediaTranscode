#pragma once
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
class MediaVideoSourceIsolationAdapter final {
public:
    static ::media::Result<MediaVideoFilterIsolationEvidence> inspect(MediaHardwareDeviceKind device);
};
} // namespace media::ffmpeg::graph
