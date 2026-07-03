#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaInputVideoStreamInfo {
    int streamIndex = invalidMediaStreamIndex;
    std::string codecName;
    int width = 0;
    int height = 0;
    MediaRational frameRate;
};

class MediaPipelineCapabilityScanner final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> detectInputVideoStreamInfo(const std::string& inputPath);

    static std::vector<MediaPipelineChainPlan> enumerateVideoTranscodeCandidates(
        const std::string& inputCodecName,
        const std::string& outputCodecName,
        const MediaPipelinePlannerOptions& options);

private:
    MediaPipelineCapabilityScanner() = default;
};

} // namespace media::ffmpeg::graph
