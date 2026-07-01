#include "internal/graph/planner/MediaPipelineGraphBuilder.h"

#include <string>

namespace media::ffmpeg::graph {

namespace {

::media::Status setOptionChecked(MediaGraph& graph,
                                 MediaNodeId nodeId,
                                 const std::string& key,
                                 const std::string& value)
{
    if (!graph.setNodeOption(nodeId, key, value)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("MediaPipelineGraphBuilder failed to set option: " + key));
    }
    return ::media::Status::success();
}

::media::Status applyStageOptions(MediaGraph& graph,
                                  MediaNodeId nodeId,
                                  const MediaPipelineStagePlan& stage,
                                  const MediaPipelineChainPlan& chain)
{
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.chain", chain.label); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.chain_score", std::to_string(chain.score)); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.stage", mediaPipelineStageRoleName(stage.role)); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.component", stage.componentName); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.available", stage.available ? "1" : "0"); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.zero_copy", stage.zeroCopy ? "1" : "0"); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.hardware", stage.hardware ? "1" : "0"); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.hwaccel", stage.hwaccelName); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.device", mediaHardwareDeviceKindName(stage.deviceKind)); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.frame_kind", mediaHardwareFrameKindName(stage.frameKind)); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.pixel_format", stage.pixelFormat); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.hw_frames_format", stage.hardwareFramesFormat); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.surface_pixel_format", stage.surfacePixelFormat); !status) return status;
    if (auto status = setOptionChecked(graph, nodeId, "pipeline.availability_reason", stage.availabilityReason); !status) return status;

    if (!stage.codecName.empty()) {
        if (auto status = setOptionChecked(graph, nodeId, "codec", stage.codecName); !status) return status;
    }
    if (!stage.ffmpegName.empty()) {
        const char* key = stage.role == MediaPipelineStageRole::Decoder ? "decoder" : "encoder";
        if (auto status = setOptionChecked(graph, nodeId, key, stage.ffmpegName); !status) return status;
    }
    if (!stage.filterName.empty()) {
        if (auto status = setOptionChecked(graph, nodeId, "filter", stage.filterName); !status) return status;
    }

    return ::media::Status::success();
}

} // namespace

::media::Status MediaPipelineGraphBuilder::applyVideoPlanToGraph(MediaGraph& graph,
                                                                 MediaNodeId videoDecodeNode,
                                                                 MediaNodeId videoFilterNode,
                                                                 MediaNodeId videoEncodeNode,
                                                                 const MediaPipelinePlan& plan)
{
    if (plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("MediaPipelineGraphBuilder requires transcode_frame video branch"));
    }

    auto decodeStatus = applyStageOptions(graph, videoDecodeNode, plan.selected.decoder, plan.selected);
    if (!decodeStatus) {
        return decodeStatus;
    }

    auto filterStatus = applyStageOptions(graph, videoFilterNode, plan.selected.filter, plan.selected);
    if (!filterStatus) {
        return filterStatus;
    }

    return applyStageOptions(graph, videoEncodeNode, plan.selected.encoder, plan.selected);
}

} // namespace media::ffmpeg::graph
