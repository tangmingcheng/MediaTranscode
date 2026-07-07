#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaPipelineCapabilityScanner final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> detectInputVideoStreamInfo(const std::string& inputPath);
    static ::media::Result<MediaInputVideoStreamInfo> detectRealtimeVideoStreamInfo(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options);

    static std::vector<MediaPipelineChainPlan> enumerateVideoTranscodeCandidates(
        const std::string& inputCodecName,
        const std::string& outputCodecName,
        const MediaPipelinePlannerOptions& options);

private:
    MediaPipelineCapabilityScanner() = default;
};

} // namespace media::ffmpeg::graph
