#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaPipelineStageRole {
    Decoder,
    Filter,
    Encoder
};

struct MediaPipelineStagePlan {
    MediaPipelineStageRole role = MediaPipelineStageRole::Decoder;
    std::string componentName;
    std::string codecName;
    std::string ffmpegName;
    std::string filterName;
    std::string hwaccelName;
    std::string pixelFormat;
    std::string hardwareFramesFormat;
    std::string surfacePixelFormat;
    MediaHardwareDeviceKind deviceKind = MediaHardwareDeviceKind::None;
    MediaHardwareFrameKind frameKind = MediaHardwareFrameKind::Software;
    bool hardware = false;
    bool zeroCopy = false;
    bool available = false;
    int score = 0;
    std::string availabilityReason;
};

struct MediaPipelineChainPlan {
    std::string label;
    MediaPipelineStagePlan decoder;
    MediaPipelineStagePlan filter;
    MediaPipelineStagePlan encoder;
    int score = 0;
    bool available = false;
    bool allHardware = false;
    bool sameHardwareDevice = false;
    bool zeroCopy = false;
    std::string reason;
};

struct MediaPipelinePlannerOptions {
    bool includeVideo = true;
    bool allowPacketCopy = false;
    std::string outputPath;
    std::string outputCodecName = "h264";
    std::string preferredHardware = "auto";
    int targetWidth = 0;
    int targetHeight = 0;
    bool filterRequired = true;
    bool preferGpu = true;
    bool allowSoftwareFallback = true;
    bool requireRuntimeAvailability = true;
    bool diagnosticLogEnabled = false;
};

struct MediaPipelinePlan {
    bool enabled = false;
    MediaBranchMode branchMode = MediaBranchMode::Drop;
    int sourceStreamIndex = invalidMediaStreamIndex;
    std::string reason;
    std::string inputPath;
    std::string outputPath;
    std::string inputCodecName;
    std::string outputCodecName;
    bool diagnosticLogEnabled = false;
    MediaPipelineChainPlan selected;
    std::vector<MediaPipelineChainPlan> candidates;
};

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept;
const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept;
const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept;

class MediaPipelinePlanner final {
public:
    static ::media::Result<MediaPipelinePlan> planVideoTranscodeFile(
        const std::string& inputPath,
        MediaPipelinePlannerOptions options = {});

private:
    MediaPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
