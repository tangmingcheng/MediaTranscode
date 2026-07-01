#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
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
    std::string inputPath;
    std::string outputPath;
    std::string inputCodecName;
    std::string outputCodecName;
    bool diagnosticLogEnabled = false;
    MediaPipelineChainPlan selected;
    std::vector<MediaPipelineChainPlan> candidates;
};

struct MediaPipelineGraphBuildResult {
    MediaGraph graph;
    MediaPipelinePlan plan;
    MediaNodeId fileInputNode;
    MediaNodeId demuxNode;
    MediaNodeId streamSplitNode;
    MediaNodeId videoDecodeNode;
    MediaNodeId videoFilterNode;
    MediaNodeId videoEncodeNode;
    MediaNodeId fileMuxNode;
    MediaNodeId fileOutputNode;
};

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept;
const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept;
const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept;

class MediaPipelinePlanner final {
public:
    static ::media::Result<MediaPipelinePlan> planVideoTranscodeFile(
        const std::string& inputPath,
        MediaPipelinePlannerOptions options = {});

    static ::media::Result<MediaPipelineGraphBuildResult> buildPlannedVideoFileTranscodeGraph(
        const std::string& inputPath,
        MediaPipelinePlannerOptions options = {});

    static ::media::Status applyVideoPlanToGraph(MediaGraph& graph,
                                                 MediaNodeId videoDecodeNode,
                                                 MediaNodeId videoFilterNode,
                                                 MediaNodeId videoEncodeNode,
                                                 const MediaPipelinePlan& plan);

private:
    MediaPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
