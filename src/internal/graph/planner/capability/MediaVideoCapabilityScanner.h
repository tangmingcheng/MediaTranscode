#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include <string>
#include <vector>
namespace media::ffmpeg::graph {
class MediaVideoCapabilityScanner final {
public:
    static std::vector<MediaPipelineChainPlan> enumerateTranscodeCandidates(const std::string& inputCodecName, const std::string& outputCodecName, const MediaPipelinePlannerOptions& options);
private:
    MediaVideoCapabilityScanner() = delete;
};
}
