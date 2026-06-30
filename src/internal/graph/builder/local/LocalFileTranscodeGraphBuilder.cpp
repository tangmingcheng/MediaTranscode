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

    graph.setNodeOption(fileInput, "url", options.inputUrl);
    graph.setNodeOption(fileOutput, "url", options.outputUrl);
    graph.setNodeOption(mux, "mux.expect_video", "1");
    graph.setNodeOption(mux, "mux.expect_audio", "0");
    if (!options.outputFormat.empty()) {
        graph.setNodeOption(fileOutput, "format", options.outputFormat);
    }

    graph.addOutputPort(fileInput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true);
    graph.addInputPort(demux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    graph.addOutputPort(demux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(split, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(fileOutput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(mux, "codec", MediaStreamKind::Any, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true);
    graph.addInputPort(mux, "packet", MediaStreamKind::Any, MediaEdgeKind::Unknown, MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", demux, "format", "local.file.input.format -> local.demux.format", q(queues.metadata));
    graph.connect(demux, "packet", split, "packet", "local.demux.packet -> local.stream.split.packet", q(queues.packet));
    graph.connect(fileOutput, "format", mux, "format", "local.file.output.format -> local.file.mux.format", q(queues.metadata));

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
    applySelectedVideoPlanOptions(graph, plannerNodes, videoPlan);

    graph.addInputPort(codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    graph.addOutputPort(codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(split, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.addInputPort(videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(videoTimestamp, "source_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    graph.addInputPort(videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", codecResolver, "format", "local.file.input.format -> local.codec.resolver.format", q(queues.metadata));
    graph.connect(codecResolver, "decoder", videoDecode, "codec", "local.codec.resolver.decoder -> local.video.decode.codec", q(queues.metadata));
    graph.connect(codecResolver, "timestamp_source", videoTimestamp, "source_codec", "local.codec.resolver.timestamp_source -> local.video.timestamp.source_codec", q(queues.metadata));
    graph.connect(codecResolver, "encoder", videoTimestamp, "target_codec", "local.codec.resolver.encoder -> local.video.timestamp.target_codec", q(queues.metadata));
    graph.connect(videoTimestamp, "target_codec", videoFilter, "codec", "local.video.timestamp.target_codec -> local.video.filter.codec", q(queues.metadata));
    graph.connect(videoFilter, "codec", videoEncode, "codec", "local.video.filter.codec -> local.video.encode.codec", q(queues.metadata));
    graph.connect(videoFilter, "codec", mux, "codec", "local.video.filter.codec -> local.file.mux.codec", q(queues.metadata));
    graph.connect(split, "video", videoDecode, "packet", "local.stream.split.video -> local.video.decode.packet", q(queues.packet));
    graph.connect(videoDecode, "frame", hardwareTransfer, "frame", "local.video.decode.frame -> local.video.hwtransfer.frame", q(queues.frame));
    graph.connect(hardwareTransfer, "frame", videoTimestamp, "frame", "local.video.hwtransfer.frame -> local.video.timestamp.frame", q(queues.frame));
    graph.connect(videoTimestamp, "frame", videoFrameRate, "frame", "local.video.timestamp.frame -> local.video.framerate.frame", q(queues.frame));
    graph.connect(videoFrameRate, "frame", videoFilter, "frame", "local.video.framerate.frame -> local.video.filter.frame", q(queues.frame));
    graph.connect(videoFilter, "frame", videoEncode, "frame", "local.video.filter.frame -> local.video.encode.frame", q(queues.frame));
    graph.connect(videoEncode, "packet", mux, "packet", "local.video.encode.packet -> local.file.mux.packet", q(queues.mux));

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
