#pragma once

#include "internal/graph/planner/video/MediaPipelineStagePlan.h"

namespace media::ffmpeg::graph {

// Planning facts only: no encoder intent or physical capability claim.
struct MediaVideoSourcePlanningOptions {
    MediaSize sourceSize;
    std::optional<MediaSize> targetSize;
    MediaRational sourceFrameRate;
    bool lowLatency;
};

struct MediaVideoSourcePlan {
    std::string label;
    MediaPipelineStagePlan decoder;
    MediaPipelineStagePlan filter;
    int score = 0;
    bool available = false;
    bool allHardware = false;
    bool sameHardwareDevice = false;
    bool zeroCopy = false;
    bool filterActive = false;
    MediaHardwareTransferDirection transferDirection = MediaHardwareTransferDirection::Unknown;
    MediaVideoLineagePropagation decoderLineagePropagation =
        MediaVideoLineagePropagation::Unknown;
    MediaVideoFilterImplementation filterImplementation =
        MediaVideoFilterImplementation::Unknown;
    std::optional<MediaRunningTime> decoderReceiveInterval;
    std::string reason;
};

} // namespace media::ffmpeg::graph
