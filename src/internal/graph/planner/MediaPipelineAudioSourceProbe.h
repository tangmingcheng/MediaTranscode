#pragma once

#include "internal/graph/model/MediaFormatDescriptor.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaPipelineAudioSourceProbeResult {
    bool found = false;
    int streamIndex = invalidMediaStreamIndex;
    std::string codecName;
    MediaFormatDescriptor descriptor;
};

class MediaPipelineAudioSourceProbe final {
public:
    static ::media::Result<MediaPipelineAudioSourceProbeResult> probeFile(const std::string& inputPath);

private:
    MediaPipelineAudioSourceProbe() = default;
};

} // namespace media::ffmpeg::graph
