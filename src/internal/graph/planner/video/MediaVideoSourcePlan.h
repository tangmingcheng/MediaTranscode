#pragma once
#include "internal/graph/planner/video/MediaVideoFrameOwnershipStoragePlanner.h"
#include "internal/graph/model/MediaVideoFilterExecutionPlan.h"

#include "internal/graph/planner/video/MediaPipelineStagePlan.h"
#include "internal/graph/model/MediaVideoSourceTiming.h"
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"
#include "internal/graph/planner/capability/MediaDecoderOutputAllocationAdapter.h"

namespace media::ffmpeg::graph {

// Planning facts only: no encoder intent or physical capability claim.
struct MediaVideoSourcePlanningOptions {
    MediaSize sourceSize;
    std::optional<MediaSize> targetSize;
    MediaVideoSourceTiming timing;
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
    std::optional<MediaDecoderOutputAllocation> decoderOutputAllocation;
    std::optional<MediaVideoFrameProducerFacts> decoderReadiness;
    std::optional<MediaVideoFrameProducerFacts> filterReadiness;
    std::optional<MediaVideoFilterExecutionPlan> filterExecution;
    std::optional<MediaVideoFrameOwnershipStoragePlan> decoderOwnershipStorage;
    std::optional<MediaVideoFrameOwnershipStoragePlan> filterOwnershipStorage;
};

} // namespace media::ffmpeg::graph
