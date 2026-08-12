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

const char* transferDirectionName(MediaHardwareTransferDirection direction) noexcept
{
    switch (direction) {
    case MediaHardwareTransferDirection::Unknown:
        return "unknown";
    case MediaHardwareTransferDirection::None:
        return "none";
    case MediaHardwareTransferDirection::Upload:
        return "upload";
    case MediaHardwareTransferDirection::Download:
        return "download";
    case MediaHardwareTransferDirection::Map:
        return "map";
    case MediaHardwareTransferDirection::Unmap:
        return "unmap";
    }
    return "unknown";
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setFrameContractOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const std::string& prefix,
    const std::optional<MediaHardwareDescriptor>& contract)
{
    if (auto status = setOption(graph, nodeId, prefix + ".present", boolOption(contract.has_value())); !status) return status;
    if (!contract) {
        return ::media::Result<void>::success();
    }
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(contract->deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(contract->frameKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".device_name", contract->deviceName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".pixel_format", contract->pixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".surface_pixel_format", contract->surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frames_context_name", contract->framesContextName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".transfer_direction", transferDirectionName(contract->transferDirection)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(contract->zeroCopyPreferred)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".requires_hw_device_ctx", boolOption(contract->requiresHardwareDeviceContext)); !status) return status;
    return setOption(graph, nodeId, prefix + ".requires_hw_frames_ctx", boolOption(contract->requiresHardwareFramesContext));
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
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind())); !status) return status;
    const auto* contract = stage.frameContract();
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(contract ? contract->frameKind : MediaHardwareFrameKind::Unknown)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hardware", boolOption(stage.hardware())); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy())); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".priority", std::to_string(stage.priority)); !status) return status;
    if (auto status = setFrameContractOptions(graph, nodeId, prefix + ".input", stage.inputFrame); !status) return status;
    return setFrameContractOptions(graph, nodeId, prefix + ".output", stage.outputFrame);
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
    if (auto status = setOption(graph, nodeId, "pipeline.filter_active", boolOption(plan.filterActive)); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "filter.pipeline", chain.filter); !status) return status;
    return setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder);
}

::media::Result<void> setCodecResolverEncoderFormatOptions(MediaGraph& graph,
                                                           MediaNodeId codecResolver,
                                                           const MediaPipelineStagePlan& encoder)
{
    if (!encoder.inputFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoPlanOptionApplier requires encoder input frame contract"));
    }
    const MediaHardwareDescriptor& input = *encoder.inputFrame;
    if (auto status = setOption(graph, codecResolver, "encoder.pixel_format", input.pixelFormat); !status) return status;
    if (auto status = setOption(graph, codecResolver, "encoder.hw_frames_format", input.requiresHardwareFramesContext ? input.pixelFormat : std::string()); !status) return status;
    if (auto status = setOption(graph, codecResolver, "encoder.surface_pixel_format", input.surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, codecResolver, "encoder.requires_hw_device_ctx", boolOption(input.requiresHardwareDeviceContext)); !status) return status;
    return setOption(graph, codecResolver, "encoder.requires_hw_frames_ctx", boolOption(input.requiresHardwareFramesContext));
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
        if (!nodeId.isValid()) {
            continue;
        }
        if (auto status = setFullPlanOptions(graph, nodeId, plan); !status) return status;
    }

    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedDecoder, chain.decoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::VideoCodec, plan.outputCodecName); !status) return status;
    if (auto status = setCodecResolverEncoderFormatOptions(graph, nodes.codecResolver, chain.encoder); !status) return status;
    if (!chain.decoder.outputFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoPlanOptionApplier requires decoder output frame contract"));
    }
    const MediaHardwareDescriptor& decoderOutput = *chain.decoder.outputFrame;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hardware", boolOption(decoderOutput.isHardwareBacked())); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hwaccel", chain.decoder.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.device", mediaHardwareDeviceKindName(decoderOutput.deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.frame_kind", mediaHardwareFrameKindName(decoderOutput.frameKind)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "decoder.output.pixel_format", decoderOutput.pixelFormat); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "decoder.output.surface_pixel_format", decoderOutput.surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "decoder.output.requires_hw_device_ctx", boolOption(decoderOutput.requiresHardwareDeviceContext)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "decoder.output.requires_hw_frames_ctx", boolOption(decoderOutput.requiresHardwareFramesContext)); !status) return status;
    if (chain.transferDirection == MediaHardwareTransferDirection::Unknown) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoPlanOptionApplier requires planner-selected transfer direction"));
    }
    if (auto status = setOption(graph, nodes.hardwareTransfer, "transfer.direction", transferDirectionName(chain.transferDirection)); !status) return status;
    if (nodes.videoFilter.isValid()) {
        if (auto status = setOption(graph, nodes.videoFilter, MediaTranscodeOptionKey::PlannedFilter, chain.filter.filterName); !status) return status;
        if (auto status = setOption(graph, nodes.videoFilter, "filter.name", chain.filter.filterName); !status) return status;
        if (auto status = setOption(graph, nodes.videoFilter, "filter.hwaccel", chain.filter.hwaccelName); !status) return status;
    }
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = setOption(graph, nodes.videoTimestamp, MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps, boolOption(plan.synthesizeMissingTimestamps)); !status) return status;
    }
    return setOption(graph, nodes.videoEncode, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName);
}

} // namespace media::ffmpeg::graph
