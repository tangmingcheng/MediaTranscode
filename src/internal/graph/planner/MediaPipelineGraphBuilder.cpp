#include "internal/graph/planner/MediaPipelineGraphBuilder.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

bool setOption(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    return graph.setNodeOption(nodeId, key, value);
}

::media::Status applyStageOptions(MediaGraph& graph,
                                  MediaNodeId nodeId,
                                  const MediaPipelineStagePlan& stage,
                                  const MediaPipelineChainPlan& chain)
{
    bool ok = true;
    ok = ok && setOption(graph, nodeId, "pipeline.chain", chain.label);
    ok = ok && setOption(graph, nodeId, "pipeline.chain_score", std::to_string(chain.score));
    ok = ok && setOption(graph, nodeId, "pipeline.stage", mediaPipelineStageRoleName(stage.role));
    ok = ok && setOption(graph, nodeId, "pipeline.component", stage.componentName);
    ok = ok && setOption(graph, nodeId, "pipeline.available", stage.available ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.zero_copy", stage.zeroCopy ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.hardware", stage.hardware ? "1" : "0");
    ok = ok && setOption(graph, nodeId, "pipeline.hwaccel", stage.hwaccelName);
    ok = ok && setOption(graph, nodeId, "pipeline.device", mediaHardwareDeviceKindName(stage.deviceKind));
    ok = ok && setOption(graph, nodeId, "pipeline.frame_kind", mediaHardwareFrameKindName(stage.frameKind));
    ok = ok && setOption(graph, nodeId, "pipeline.availability_reason", stage.availabilityReason);

    if (!stage.codecName.empty()) {
        ok = ok && setOption(graph, nodeId, "codec", stage.codecName);
    }
    if (!stage.ffmpegName.empty()) {
        const char* key = stage.role == MediaPipelineStageRole::Decoder ? "decoder" : "encoder";
        ok = ok && setOption(graph, nodeId, key, stage.ffmpegName);
    }
    if (!stage.filterName.empty()) {
        ok = ok && setOption(graph, nodeId, "filter", stage.filterName);
    }

    if (!ok) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("failed to apply media pipeline stage options"));
    }

    return ::media::Status::success();
}

MediaEdgePolicy queuePolicy(std::size_t capacity = 256)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    return policy;
}

} // namespace

::media::Status MediaPipelineGraphBuilder::applyVideoPlanToGraph(MediaGraph& graph,
                                                                 MediaNodeId videoDecodeNode,
                                                                 MediaNodeId videoFilterNode,
                                                                 MediaNodeId videoEncodeNode,
                                                                 const MediaPipelinePlan& plan)
{
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

::media::Result<MediaPipelineGraphBuildResult> MediaPipelineGraphBuilder::buildVideoFileTranscodeGraph(
    MediaPipelinePlan plan)
{
    MediaPipelineGraphBuildResult result;
    result.plan = std::move(plan);

    MediaGraph graph;
    result.fileInputNode = graph.addNode(MediaNodeKind::FileInput, "file-input");
    result.demuxNode = graph.addNode(MediaNodeKind::Demux, "demux");
    result.streamSplitNode = graph.addNode(MediaNodeKind::StreamSplit, "stream-split");
    result.videoDecodeNode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    result.videoFilterNode = graph.addNode(MediaNodeKind::VideoFilter, "video-filter");
    result.videoEncodeNode = graph.addNode(MediaNodeKind::VideoEncode, "video-encode");
    result.fileOutputNode = graph.addNode(MediaNodeKind::FileOutput, "file-output");
    result.fileMuxNode = graph.addNode(MediaNodeKind::FileMux, "file-mux");

    graph.setNodeOption(result.fileInputNode, "path", result.plan.inputPath);
    graph.setNodeOption(result.fileOutputNode,
                        "path",
                        result.plan.outputPath.empty() ? "planned-output.mp4" : result.plan.outputPath);

    graph.addOutputPort(result.fileInputNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.demuxNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext, true, false);

    graph.addOutputPort(result.demuxNode, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);
    graph.addInputPort(result.streamSplitNode, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(result.streamSplitNode, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);

    graph.addInputPort(result.videoDecodeNode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(result.videoDecodeNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);

    graph.addInputPort(result.videoFilterNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(result.videoFilterNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);

    graph.addInputPort(result.videoEncodeNode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(result.videoEncodeNode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                        MediaPayloadKind::Packet, true, true);

    graph.addOutputPort(result.fileOutputNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.fileMuxNode, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(result.fileMuxNode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                       MediaPayloadKind::Packet, true, true);

    graph.connect(result.fileInputNode, "format", result.demuxNode, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(result.demuxNode, "packet", result.streamSplitNode, "packet", "demux-to-stream-split", queuePolicy(256));
    graph.connect(result.streamSplitNode, "video", result.videoDecodeNode, "packet", "stream-split-to-video-decode", queuePolicy(256));
    graph.connect(result.videoDecodeNode, "frame", result.videoFilterNode, "frame", "video-decode-to-filter", queuePolicy(256));
    graph.connect(result.videoFilterNode, "frame", result.videoEncodeNode, "frame", "video-filter-to-encode", queuePolicy(256));
    graph.connect(result.fileOutputNode, "format", result.fileMuxNode, "format", "file-output-to-mux", queuePolicy(1));
    graph.connect(result.videoEncodeNode, "packet", result.fileMuxNode, "packet", "video-encode-to-mux", queuePolicy(256));

    auto applyStatus = applyVideoPlanToGraph(graph,
                                             result.videoDecodeNode,
                                             result.videoFilterNode,
                                             result.videoEncodeNode,
                                             result.plan);
    if (!applyStatus) {
        return ::media::Result<MediaPipelineGraphBuildResult>::failure(applyStatus.error());
    }

    result.graph = std::move(graph);
    return ::media::Result<MediaPipelineGraphBuildResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
