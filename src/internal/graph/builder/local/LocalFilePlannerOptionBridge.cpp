#include "internal/graph/builder/local/LocalFilePlannerOptionBridge.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

void setStageOptions(MediaGraph& graph,
                     MediaNodeId nodeId,
                     const std::string& prefix,
                     const MediaPipelineStagePlan& stage)
{
    graph.setNodeOption(nodeId, prefix + ".component", stage.componentName);
    graph.setNodeOption(nodeId, prefix + ".codec", stage.codecName);
    graph.setNodeOption(nodeId, prefix + ".ffmpeg", stage.ffmpegName);
    graph.setNodeOption(nodeId, prefix + ".filter", stage.filterName);
    graph.setNodeOption(nodeId, prefix + ".hwaccel", stage.hwaccelName);
    graph.setNodeOption(nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind));
    graph.setNodeOption(nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(stage.frameKind));
    graph.setNodeOption(nodeId, prefix + ".hardware", boolOption(stage.hardware));
    graph.setNodeOption(nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy));
    graph.setNodeOption(nodeId, prefix + ".score", std::to_string(stage.score));
}

void setChainOptions(MediaGraph& graph,
                     MediaNodeId nodeId,
                     const MediaPipelineChainPlan& chain)
{
    graph.setNodeOption(nodeId, "pipeline.chain", chain.label);
    graph.setNodeOption(nodeId, "pipeline.score", std::to_string(chain.score));
    graph.setNodeOption(nodeId, "pipeline.zero_copy", boolOption(chain.zeroCopy));
    graph.setNodeOption(nodeId, "pipeline.all_hardware", boolOption(chain.allHardware));
    graph.setNodeOption(nodeId, "pipeline.same_hardware_device", boolOption(chain.sameHardwareDevice));
    graph.setNodeOption(nodeId, "pipeline.reason", chain.reason);
}

void setFullPlanOptions(MediaGraph& graph,
                        MediaNodeId nodeId,
                        const MediaPipelineChainPlan& chain)
{
    setChainOptions(graph, nodeId, chain);
    setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder);
    setStageOptions(graph, nodeId, "filter.pipeline", chain.filter);
    setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder);
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

void applySelectedVideoPlanOptions(MediaGraph& graph,
                                   const LocalFilePlannerNodeIds& nodes,
                                   const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& chain = plan.selected;

    setFullPlanOptions(graph, nodes.codecResolver, chain);
    setFullPlanOptions(graph, nodes.videoDecode, chain);
    setFullPlanOptions(graph, nodes.hardwareTransfer, chain);
    setFullPlanOptions(graph, nodes.videoTimestamp, chain);
    setFullPlanOptions(graph, nodes.videoFrameRate, chain);
    setFullPlanOptions(graph, nodes.videoFilter, chain);
    setFullPlanOptions(graph, nodes.videoEncode, chain);

    graph.setNodeOption(nodes.codecResolver, "decoder", chain.decoder.ffmpegName);
    graph.setNodeOption(nodes.codecResolver, "encoder", chain.encoder.ffmpegName);
    graph.setNodeOption(nodes.codecResolver, "video_codec", plan.outputCodecName);

    graph.setNodeOption(nodes.hardwareTransfer, "transfer.direction", transferDirectionForPlan(chain));

    graph.setNodeOption(nodes.videoFilter, "filter", chain.filter.filterName);
    graph.setNodeOption(nodes.videoFilter, "filter.name", chain.filter.filterName);
    graph.setNodeOption(nodes.videoFilter, "filter.hwaccel", chain.filter.hwaccelName);

    graph.setNodeOption(nodes.videoEncode, "encoder", chain.encoder.ffmpegName);
}

} // namespace media::ffmpeg::graph
