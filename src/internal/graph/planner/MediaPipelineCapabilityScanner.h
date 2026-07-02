#pragma once

#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaPipelineCapabilityScanner final {
public:
    static ::media::Result<std::string> detectInputVideoCodecName(const std::string& inputPath);

    static std::vector<MediaPipelineChainPlan> enumerateVideoTranscodeCandidates(
        const std::string& inputCodecName,
        const std::string& outputCodecName,
        const MediaPipelinePlannerOptions& options);

private:
    MediaPipelineCapabilityScanner() = default;
};

} // namespace media::ffmpeg::graph
