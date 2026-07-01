#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

#include "internal/graph/builder/local/LocalFileAudioBranchBuilder.h"
#include "internal/graph/builder/local/LocalFileNodeOptionApplier.h"
#include "internal/graph/builder/local/LocalFilePlannerOptionBridge.h"
#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaEdgePolicy q(std::size_t capacity)
{
    MediaEdgePolicy p;
    p.queuePolicy.mode = MediaQueueMode::Blocking;
    p.queuePolicy.bounded = true;
    p.queuePolicy.capacity = capacity;
    p.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    p.queuePolicy.preserveOrdering = true;
    p.queuePolicy.allowFlushControlBypass = true;
    p.queuePolicy.collectMetrics = true;
    return p;
}

::media::Result<MediaPipelinePlan> videoPlanFor(const LocalFileTranscodeOptions& options)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildVideoPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<MediaPipelinePlan>::failure(plannerOptions.error());
    }
    return MediaPipelinePlanner::planVideoTranscodeFile(options.inputUrl, std::move(plannerOptions).value());
}

::media::Result<void> setNodeOptionChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    if (!graph.setNodeOption(nodeId, key, value)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError("LocalFileTranscodeGraphBuilder failed to set option: " + key));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> requirePort(MediaPortId portId, const char* name)
{
    if (!portId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(std::string("LocalFileTranscodeGraphBuilder failed to add port: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> requireEdge(MediaEdgeId edgeId, const char* name)
{
    if (!edgeId.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError(std::string("LocalFileTranscodeGraphBuilder failed to connect edge: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> addInputPortChecked(MediaGraph& graph,
                                          MediaNodeId nodeId,
                                          const std::string& name,
                                          MediaStreamKind streamKind,
                                          MediaEdgeKind edgeKind,
                                          MediaPayloadKind payloadKind,
                                          bool required,
                                          bool multiple)
{
    return requirePort(graph.addInputPort(nodeId, name, streamKind, edgeKind, payloadKind, required, multiple), name.c_str());
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph,
                                           MediaNodeId nodeId,
                                           const std::string& name,
                                           MediaStreamKind streamKind,
                                           MediaEdgeKind edgeKind,
                                           MediaPayloadKind payloadKind,
                                           bool required,
                                           bool multiple)
{
    return requirePort(graph.addOutputPort(nodeId, name, streamKind, edgeKind, payloadKind, required, multiple), name.c_str());
}

::media::Result<void> connectChecked(MediaGraph& graph,
                                     MediaNodeId fromNode,
                                     const std::string& fromPort,
                                     MediaNodeId toNode,
                                     const std::string& toPort,
                                     const std::string& label,
                                     const MediaEdgePolicy& policy)
{
    return requireEdge(graph.connect(fromNode, fromPort, toNode, toPort, label, policy), label.c_str());
}

} // namespace

::media::Status LocalFileTranscodeGraphBuilder::validate(const LocalFileTranscodeOptions& options)
{
    const MediaTranscodeParameterSet& parameters = options.parameters;
    if (options.inputUrl.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires inputUrl"));
    }
    if (options.outputUrl.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires outputUrl"));
    }
    if (!parameters.execution.includeVideo && !parameters.execution.includeAudio) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires video or audio branch"));
    }
    if (!parameters.execution.includeVideo) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported("LocalFileTranscodeGraphBuilder audio-only graph requires video branch planning"));
    }
    if (parameters.queues.metadata == 0 || parameters.queues.packet == 0 ||
        parameters.queues.frame == 0 || parameters.queues.mux == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder queue capacities must be greater than 0"));
    }
    return ::media::Status::success();
}

::media::Result<MediaGraph> LocalFileTranscodeGraphBuilder::build(const LocalFileTranscodeOptions& options)
{
    auto validation = validate(options);
    if (!validation) {
        return ::media::Result<MediaGraph>::failure(validation.error());
    }

    auto plannedVideo = videoPlanFor(options);
    if (!plannedVideo) {
        return ::media::Result<MediaGraph>::failure(plannedVideo.error());
    }
    MediaPipelinePlan videoPlan = std::move(plannedVideo).value();
    const MediaGraphQueueParameters& queues = options.parameters.queues;

    MediaGraph graph;
    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "local.file.input", "Local file input");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "local.demux", "Local demux");
    const MediaNodeId split = graph.addNode(MediaNodeKind::StreamSplit, "local.stream.split", "Local stream split");
    const MediaNodeId fileOutput = graph.addNode(MediaNodeKind::FileOutput, "local.file.output", "Local file output");
    const MediaNodeId mux = graph.addNode(MediaNodeKind::FileMux, "local.file.mux", "Local file mux");

    if (auto status = setNodeOptionChecked(graph, fileInput, "url", options.inputUrl); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = setNodeOptionChecked(graph, fileOutput, "url", options.outputUrl); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectVideo, "1"); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectAudio, "0"); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (!options.outputFormat.empty()) {
        if (auto status = setNodeOptionChecked(graph, fileOutput, "format", options.outputFormat); !status) return ::media::Result<MediaGraph>::failure(status.error());
    }

    if (auto status = addOutputPortChecked(graph, fileInput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, demux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, demux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, split, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, fileOutput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, mux, "codec", MediaStreamKind::Any, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, mux, "packet", MediaStreamKind::Any, MediaEdgeKind::Unknown, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = connectChecked(graph, fileInput, "format", demux, "format", "local.file.input.format -> local.demux.format", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, demux, "packet", split, "packet", "local.demux.packet -> local.stream.split.packet", q(queues.packet)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, fileOutput, "format", mux, "format", "local.file.output.format -> local.file.mux.format", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());

    auto audio = LocalFileAudioBranchBuilder::buildIfPlanned(graph, options, fileInput, split, mux);
    if (!audio) {
        return ::media::Result<MediaGraph>::failure(audio.error());
    }

    const MediaNodeId codecResolver = graph.addNode(MediaNodeKind::CodecResolver, "local.codec.resolver", "Local codec resolver");
    const MediaNodeId videoDecode = graph.addNode(MediaNodeKind::VideoDecode, "local.video.decode", "Local video decode");
    const MediaNodeId hardwareTransfer = graph.addNode(MediaNodeKind::HardwareTransfer, "local.video.hwtransfer", "Local hardware frame transfer");
    const MediaNodeId videoTimestamp = graph.addNode(MediaNodeKind::VideoTimestamp, "local.video.timestamp", "Local video timestamp normalize");
    const MediaNodeId videoFrameRate = graph.addNode(MediaNodeKind::VideoFrameRate, "local.video.framerate", "Local video frame rate control");
    const MediaNodeId videoFilter = graph.addNode(MediaNodeKind::VideoFilter, "local.video.filter", "Local video filter");
    const MediaNodeId videoEncode = graph.addNode(MediaNodeKind::VideoEncode, "local.video.encode", "Local video encode");

    LocalFilePlannerNodeIds plannerNodes;
    plannerNodes.codecResolver = codecResolver;
    plannerNodes.videoDecode = videoDecode;
    plannerNodes.hardwareTransfer = hardwareTransfer;
    plannerNodes.videoTimestamp = videoTimestamp;
    plannerNodes.videoFrameRate = videoFrameRate;
    plannerNodes.videoFilter = videoFilter;
    plannerNodes.videoEncode = videoEncode;

    auto userOptions = LocalFileNodeOptionApplier::applyUserVideoOptions(graph, plannerNodes, options);
    if (!userOptions) {
        return ::media::Result<MediaGraph>::failure(userOptions.error());
    }
    auto planStatus = MediaPipelinePlanner::applyVideoPlanToGraph(graph, videoDecode, videoFilter, videoEncode, videoPlan);
    if (!planStatus) {
        return ::media::Result<MediaGraph>::failure(planStatus.error());
    }
    auto selectedPlanOptions = applySelectedVideoPlanOptions(graph, plannerNodes, videoPlan);
    if (!selectedPlanOptions) {
        return ::media::Result<MediaGraph>::failure(selectedPlanOptions.error());
    }

    if (auto status = addInputPortChecked(graph, codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, split, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoTimestamp, "source_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addInputPortChecked(graph, videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addOutputPortChecked(graph, videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = connectChecked(graph, fileInput, "format", codecResolver, "format", "local.file.input.format -> local.codec.resolver.format", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, codecResolver, "decoder", videoDecode, "codec", "local.codec.resolver.decoder -> local.video.decode.codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, codecResolver, "timestamp_source", videoTimestamp, "source_codec", "local.codec.resolver.timestamp_source -> local.video.timestamp.source_codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, codecResolver, "encoder", videoTimestamp, "target_codec", "local.codec.resolver.encoder -> local.video.timestamp.target_codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoTimestamp, "target_codec", videoFilter, "codec", "local.video.timestamp.target_codec -> local.video.filter.codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoFilter, "codec", videoEncode, "codec", "local.video.filter.codec -> local.video.encode.codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoFilter, "codec", mux, "codec", "local.video.filter.codec -> local.file.mux.codec", q(queues.metadata)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, split, "video", videoDecode, "packet", "local.stream.split.video -> local.video.decode.packet", q(queues.packet)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoDecode, "frame", hardwareTransfer, "frame", "local.video.decode.frame -> local.video.hwtransfer.frame", q(queues.frame)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, hardwareTransfer, "frame", videoTimestamp, "frame", "local.video.hwtransfer.frame -> local.video.timestamp.frame", q(queues.frame)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoTimestamp, "frame", videoFrameRate, "frame", "local.video.timestamp.frame -> local.video.framerate.frame", q(queues.frame)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoFrameRate, "frame", videoFilter, "frame", "local.video.framerate.frame -> local.video.filter.frame", q(queues.frame)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoFilter, "frame", videoEncode, "frame", "local.video.filter.frame -> local.video.encode.frame", q(queues.frame)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = connectChecked(graph, videoEncode, "packet", mux, "packet", "local.video.encode.packet -> local.file.mux.packet", q(queues.mux)); !status) return ::media::Result<MediaGraph>::failure(status.error());

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
