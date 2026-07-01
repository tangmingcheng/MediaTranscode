#include "internal/graph/builder/local/LocalFilePlannerOptionBridge.h"

#include "internal/graph/model/MediaTranscodeParameters.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

::media::Status setNodeOptionChecked(MediaGraph& graph,
                                      MediaNodeId nodeId,
                                      const std::string& key,
                                      const std::string& value)
{
    if (!graph.setNodeOption(nodeId, key, value)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError("LocalFilePlannerOptionBridge failed to set option: " + key));
    }
    return ::media::Status::success();
}

::media::Status setStageOptions(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& prefix,
                                const MediaPipelineStagePlan& stage)
{
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".component", stage.componentName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".codec", stage.codecName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".ffmpeg", stage.ffmpegName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".filter", stage.filterName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".hwaccel", stage.hwaccelName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(stage.frameKind)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".pixel_format", stage.pixelFormat); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".hw_frames_format", stage.hardwareFramesFormat); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".surface_pixel_format", stage.surfacePixelFormat); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".hardware", boolOption(stage.hardware)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, prefix + ".score", std::to_string(stage.score)); !status) return status;
    return ::media::Status::success();
}

::media::Status setChainOptions(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const MediaPipelineChainPlan& chain)
{
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.chain", chain.label); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.score", std::to_string(chain.score)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.zero_copy", boolOption(chain.zeroCopy)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.all_hardware", boolOption(chain.allHardware)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.same_hardware_device", boolOption(chain.sameHardwareDevice)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, "pipeline.reason", chain.reason); !status) return status;
    return ::media::Status::success();
}

::media::Status setFullPlanOptions(MediaGraph& graph,
                                   MediaNodeId nodeId,
                                   const MediaPipelineChainPlan& chain)
{
    if (auto status = setChainOptions(graph, nodeId, chain); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "filter.pipeline", chain.filter); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder); !status) return status;
    return ::media::Status::success();
}

::media::Status setCodecResolverEncoderFormatOptions(MediaGraph& graph,
                                                     MediaNodeId codecResolver,
                                                     const MediaPipelineStagePlan& encoder)
{
    if (auto status = setNodeOptionChecked(graph, codecResolver, "encoder.pixel_format", encoder.pixelFormat); !status) return status;
    if (auto status = setNodeOptionChecked(graph, codecResolver, "encoder.hw_frames_format", encoder.hardwareFramesFormat); !status) return status;
    if (auto status = setNodeOptionChecked(graph, codecResolver, "encoder.surface_pixel_format", encoder.surfacePixelFormat); !status) return status;
    return ::media::Status::success();
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

::media::Status applySelectedVideoPlanOptions(MediaGraph& graph,
                                              const LocalFilePlannerNodeIds& nodes,
                                              const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& chain = plan.selected;

    if (auto status = setFullPlanOptions(graph, nodes.codecResolver, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.videoDecode, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.hardwareTransfer, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.videoTimestamp, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.videoFrameRate, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.videoFilter, chain); !status) return status;
    if (auto status = setFullPlanOptions(graph, nodes.videoEncode, chain); !status) return status;

    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedDecoder, chain.decoder.ffmpegName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, MediaTranscodeOptionKey::VideoCodec, plan.outputCodecName); !status) return status;
    if (auto status = setCodecResolverEncoderFormatOptions(graph, nodes.codecResolver, chain.encoder); !status) return status;

    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, "pipeline.hardware", boolOption(chain.decoder.hardware)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, "pipeline.hwaccel", chain.decoder.hwaccelName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, "pipeline.device", mediaHardwareDeviceKindName(chain.decoder.deviceKind)); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.codecResolver, "pipeline.frame_kind", mediaHardwareFrameKindName(chain.decoder.frameKind)); !status) return status;

    if (auto status = setNodeOptionChecked(graph, nodes.hardwareTransfer, "transfer.direction", transferDirectionForPlan(chain)); !status) return status;

    if (auto status = setNodeOptionChecked(graph, nodes.videoFilter, MediaTranscodeOptionKey::PlannedFilter, chain.filter.filterName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.videoFilter, "filter.name", chain.filter.filterName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodes.videoFilter, "filter.hwaccel", chain.filter.hwaccelName); !status) return status;

    if (auto status = setNodeOptionChecked(graph, nodes.videoEncode, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName); !status) return status;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
