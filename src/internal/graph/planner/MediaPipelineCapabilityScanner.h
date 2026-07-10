#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo {
    MediaInputVideoStreamInfo video;
    bool hasAudio = false;
    MediaInputAudioStreamInfo audio;
};

struct MediaPreparedRealtimeInputScan final {
    MediaRealtimeInputStreamInfo streams;
    MediaPreparedRealtimeInput prepared;
};

class MediaPipelineCapabilityScanner final {
public:
    static ::media::Result<MediaInputVideoStreamInfo> detectInputVideoStreamInfo(const std::string& inputPath);
    static ::media::Result<MediaInputVideoStreamInfo> detectRealtimeVideoStreamInfo(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options);
    static ::media::Result<MediaRealtimeInputStreamInfo> detectRealtimeInputStreamInfo(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options,
        bool includeAudio);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepareRealtimeInput(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options,
        bool includeAudio);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepareRealtimeInput(
        const std::string& inputUrl,
        const MediaPipelinePlannerOptions& options,
        bool includeAudio,
        const MediaRealtimeInputOpener& opener);

    static std::vector<MediaPipelineChainPlan> enumerateVideoTranscodeCandidates(
        const std::string& inputCodecName,
        const std::string& outputCodecName,
        const MediaPipelinePlannerOptions& options);

private:
    MediaPipelineCapabilityScanner() = default;
};

} // namespace media::ffmpeg::graph
