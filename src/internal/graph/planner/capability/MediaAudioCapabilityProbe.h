#pragma once

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include <media_transcode/Result.h>

struct AVFormatContext;

namespace media::ffmpeg::graph {

struct MediaAudioCapability final {
    bool present = false;
    MediaInputAudioStreamInfo stream;
};

class MediaAudioCapabilityProbe final {
public:
    static ::media::Result<MediaAudioCapability> inspect(AVFormatContext& inputContext);

private:
    MediaAudioCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
