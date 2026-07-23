#include "internal/graph/builder/MediaVideoPlanOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <array>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoPlanOptionApplier";

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setStageOptions(MediaGraph& graph,
                                       MediaNodeId nodeId,
                                       const std::string& prefix,
                                       const MediaPipelineStagePlan& stage)
{
    if (auto status = setOption(graph, nodeId, prefix + ".component", stage.componentName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".codec", stage.codecName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".ffmpeg", stage.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".filter", stage.filterName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hwaccel", stage.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(stage.frameKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".pixel_format", stage.pixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hw_frames_format", stage.hardwareFramesFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".surface_pixel_format", stage.surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hardware", boolOption(stage.hardware)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy)); !status) return status;
    return setOption(graph, nodeId, prefix + ".priority", std::to_string(stage.priority));
}

::media::Result<void> setChainOptions(MediaGraph& graph,
                                      MediaNodeId nodeId,
                                      const MediaPipelineChainPlan& chain)
{
    if (auto status = setOption(graph, nodeId, "pipeline.chain", chain.label); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.score", std::to_string(chain.score)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.zero_copy", boolOption(chain.zeroCopy)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.all_hardware", boolOption(chain.allHardware)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.same_hardware_device", boolOption(chain.sameHardwareDevice)); !status) return status;
    return setOption(graph, nodeId, "pipeline.reason", chain.reason);
}

::media::Result<void> setFullPlanOptions(MediaGraph& graph,
                                         MediaNodeId nodeId,
                                         const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& chain = plan.selected;
    if (auto status = setChainOptions(graph, nodeId, chain); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.filter_required", boolOption(plan.filterRequired)); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "filter.pipeline", chain.filter); !status) return status;
    return setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder);
}

::media::Result<void> setCodecResolverEncoderFormatOptions(MediaGraph& graph,
                                                           MediaNodeId codecResolver,
                                                           const MediaPipelineStagePlan& encoder)
{
    if (auto status = setOption(graph, codecResolver, "encoder.pixel_format", encoder.pixelFormat); !status) return status;
    if (auto status = setOption(graph, codecResolver, "encoder.hw_frames_format", encoder.hardwareFramesFormat); !status) return status;
    return setOption(graph, codecResolver, "encoder.surface_pixel_format", encoder.surfacePixelFormat);
}

std::string transferDirectionForPlan(const MediaPipelineChainPlan& chain)
{
    if (chain.decoder.frameKind == MediaHardwareFrameKind::Hardware &&
        chain.filter.frameKind == MediaHardwareFrameKind::Software) {
        return "download";
    }
    if (chain.decoder.frameKind == MediaHardwareFrameKind::Software &&
        chain.filter.frameKind == MediaHardwareFrameKind::Hardware) {
        return "upload";
    }
    return "none";
}

} // namespace

::media::Result<void> MediaVideoPlanOptionApplier::applySelectedPlan(
    MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes,
    const MediaPipelinePlan& plan)
{
    if (plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaVideoPlanOptionApplier requires transcode_frame video branch"));
    }

    const MediaPipelineChainPlan& chain = plan.selected;
    std::vector<MediaNodeId> plannedNodes {
        nodes.codecResolver,
        nodes.videoDecode,
        nodes.hardwareTransfer,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };
    if (nodes.videoTimestamp.isValid()) {
        plannedNodes.push_back(nodes.videoTimestamp);
    }

    for (MediaNodeId nodeId : plannedNodes) {
        if (auto status = setFullPlanOptions(graph, nodeId, plan); !status) return status;
    }

    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedDecoder, chain.decoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::VideoCodec, plan.outputCodecName); !status) return status;
    if (auto status = setCodecResolverEncoderFormatOptions(graph, nodes.codecResolver, chain.encoder); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hardware", boolOption(chain.decoder.hardware)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hwaccel", chain.decoder.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.device", mediaHardwareDeviceKindName(chain.decoder.deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.frame_kind", mediaHardwareFrameKindName(chain.decoder.frameKind)); !status) return status;
    if (auto status = setOption(graph, nodes.hardwareTransfer, "transfer.direction", transferDirectionForPlan(chain)); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, MediaTranscodeOptionKey::PlannedFilter, chain.filter.filterName); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, "filter.name", chain.filter.filterName); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, "filter.hwaccel", chain.filter.hwaccelName); !status) return status;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = setOption(graph, nodes.videoTimestamp, MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps, boolOption(plan.synthesizeMissingTimestamps)); !status) return status;
    }
    return setOption(graph, nodes.videoEncode, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName);
}

} // namespace media::ffmpeg::graph
