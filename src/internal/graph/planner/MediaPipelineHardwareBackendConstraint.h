#pragma once

#include "internal/graph/model/MediaHardwareBackendRequest.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaPipelineHardwareBackendConstraint final {
public:
    static ::media::Status validate(MediaHardwareBackendRequest request,
                                    bool disableHardware,
                                    const std::string& context);

    static bool accepts(const MediaPipelineChainPlan& candidate,
                        bool filterRequired,
                        MediaHardwareBackendRequest request) noexcept;

private:
    MediaPipelineHardwareBackendConstraint() = default;
};

} // namespace media::ffmpeg::graph
