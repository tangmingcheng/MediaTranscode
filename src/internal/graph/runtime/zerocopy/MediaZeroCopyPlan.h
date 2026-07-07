#pragma once

#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaZeroCopyPolicy.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaZeroCopyPlanAction {
    KeepReference,
    MapHardwareFrame,
    UploadToHardware,
    DownloadToSoftware,
    CopySoftwareFrame,
    Unsupported
};

struct MediaZeroCopyPlanStep {
    MediaZeroCopyPlanAction action = MediaZeroCopyPlanAction::Unsupported;
    MediaInteropKind interopKind = MediaInteropKind::None;
    std::string reason;
};

struct MediaZeroCopyPlan {
    bool zeroCopy = false;
    bool softwareTransfer = false;
    std::vector<MediaZeroCopyPlanStep> steps;
};

} // namespace media::ffmpeg::graph
