#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo {
    MediaInputVideoStreamInfo video;
    bool hasAudio = false;
    MediaInputAudioStreamInfo audio;
};

class MediaPipelineCapabilityScanner final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> detectInputVideoStreamInfo(const std::string& inputPath);
    static ::media::Result<MediaInputVideoStreamInfo> detectRealtimeVideoStreamInfo(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options);
    static ::media::Result<MediaRealtimeInputStreamInfo> detectRealtimeInputStreamInfo(
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
