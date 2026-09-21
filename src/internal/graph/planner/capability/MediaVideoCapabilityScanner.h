#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include <string>
#include <vector>
namespace media::ffmpeg::graph {
class MediaVideoCapabilityScanner final {
public:
    static ::media::Result<std::string> planRkmppFilter(const MediaVideoSourcePlanningOptions& options);
    static ::media::Result<std::vector<MediaVideoSourcePlan>> enumerateSourceCandidates(
        const std::string& inputCodecName, const MediaVideoSourcePlanningOptions& options,
        const MediaHardwareDescriptor& target);
    static std::vector<MediaPipelineChainPlan> enumerateTranscodeCandidates(const std::string& inputCodecName, const std::string& outputCodecName, const MediaPipelinePlannerOptions& options);
private:
    MediaVideoCapabilityScanner() = delete;
};
}
