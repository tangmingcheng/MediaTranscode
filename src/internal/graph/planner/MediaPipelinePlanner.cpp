#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/MediaPipelineGraphBuilder.h"
#include "internal/graph/planner/MediaPipelineScorer.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

std::string canonicalCodecName(std::string codec)
{
    std::transform(codec.begin(), codec.end(), codec.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (codec == "avc" || codec == "h.264") {
        return "h264";
    }
    if (codec == "h265" || codec == "h.265") {
        return "hevc";
    }
    return codec;
}

} // namespace

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept
{
    switch (role) {
    case MediaPipelineStageRole::Decoder:
        return "decoder";
    case MediaPipelineStageRole::Filter:
        return "filter";
    case MediaPipelineStageRole::Encoder:
        return "encoder";
    }
    return "unknown";
}

const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareDeviceKind::Unknown:
        return "unknown";
    case MediaHardwareDeviceKind::None:
        return "software";
    case MediaHardwareDeviceKind::D3D11VA:
        return "d3d11va";
    case MediaHardwareDeviceKind::CUDA:
        return "cuda";
    case MediaHardwareDeviceKind::VAAPI:
        return "vaapi";
    case MediaHardwareDeviceKind::DRMPrime:
        return "drm_prime";
    case MediaHardwareDeviceKind::RKMPP:
        return "rkmpp";
    case MediaHardwareDeviceKind::VideoToolbox:
        return "videotoolbox";
    case MediaHardwareDeviceKind::MediaCodec:
        return "mediacodec";
    }
    return "unknown";
}

const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareFrameKind::Unknown:
        return "unknown";
    case MediaHardwareFrameKind::Software:
        return "software";
    case MediaHardwareFrameKind::Hardware:
        return "hardware";
    case MediaHardwareFrameKind::HardwareMapped:
        return "hardware_mapped";
    }
    return "unknown";
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeFile(
    const std::string& inputPath,
    MediaPipelinePlannerOptions options)
{
    if (inputPath.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeFile requires input path"));
    }

    auto inputCodec = MediaPipelineCapabilityScanner::detectInputVideoCodecName(inputPath);
    if (!inputCodec) {
        return ::media::Result<MediaPipelinePlan>::failure(inputCodec.error());
    }

    MediaPipelinePlan plan;
    plan.inputPath = inputPath;
    plan.outputPath = std::move(options.outputPath);
    plan.inputCodecName = canonicalCodecName(inputCodec.value());
    plan.outputCodecName = canonicalCodecName(options.outputCodecName.empty() ? plan.inputCodecName : options.outputCodecName);

    auto candidates = MediaPipelineCapabilityScanner::enumerateVideoTranscodeCandidates(
        plan.inputCodecName,
        plan.outputCodecName,
        options);
    plan.candidates = MediaPipelineScorer::scoreAndSortChains(std::move(candidates), options);

    if (plan.candidates.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("no media pipeline candidates were generated"));
    }

    auto selected = std::find_if(plan.candidates.begin(), plan.candidates.end(), [&](const MediaPipelineChainPlan& chain) {
        return !options.requireRuntimeAvailability || chain.available;
    });

    if (selected == plan.candidates.end()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::hardwareUnavailable("no available decoder/filter/encoder chain found"));
    }

    plan.selected = *selected;
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

::media::Status MediaPipelinePlanner::applyVideoPlanToGraph(MediaGraph& graph,
                                                            MediaNodeId videoDecodeNode,
                                                            MediaNodeId videoFilterNode,
                                                            MediaNodeId videoEncodeNode,
                                                            const MediaPipelinePlan& plan)
{
    return MediaPipelineGraphBuilder::applyVideoPlanToGraph(graph,
                                                            videoDecodeNode,
                                                            videoFilterNode,
                                                            videoEncodeNode,
                                                            plan);
}

::media::Result<MediaPipelineGraphBuildResult> MediaPipelinePlanner::buildPlannedVideoFileTranscodeGraph(
    const std::string& inputPath,
    MediaPipelinePlannerOptions options)
{
    auto plan = planVideoTranscodeFile(inputPath, options);
    if (!plan) {
        return ::media::Result<MediaPipelineGraphBuildResult>::failure(plan.error());
    }

    return MediaPipelineGraphBuilder::buildVideoFileTranscodeGraph(std::move(plan).value());
}

} // namespace media::ffmpeg::graph
