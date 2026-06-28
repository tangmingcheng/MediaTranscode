#include "internal/graph/builder/local/MediaLocalFileTranscodeGraphBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaEdgePolicy blockingQueuePolicy(std::size_t capacity)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    policy.queuePolicy.preserveOrdering = true;
    policy.queuePolicy.allowFlushControlBypass = true;
    policy.queuePolicy.collectMetrics = true;
    return policy;
}

} // namespace

::media::Status MediaLocalFileTranscodeGraphBuilder::validate(
    const MediaLocalFileTranscodeGraphBuilderOptions& options)
{
    if (options.inputUrl.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaLocalFileTranscodeGraphBuilder requires inputUrl"));
    }

    if (options.outputUrl.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaLocalFileTranscodeGraphBuilder requires outputUrl"));
    }

    if (!options.includeVideo && !options.includeAudio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaLocalFileTranscodeGraphBuilder requires video or audio branch"));
    }

    if (options.metadataQueueCapacity == 0 ||
        options.packetQueueCapacity == 0 ||
        options.frameQueueCapacity == 0 ||
        options.muxQueueCapacity == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaLocalFileTranscodeGraphBuilder queue capacities must be greater than 0"));
    }

    return ::media::Status::success();
}

::media::Result<MediaGraph> MediaLocalFileTranscodeGraphBuilder::build(
    const MediaLocalFileTranscodeGraphBuilderOptions& options)
{
    auto validation = validate(options);
    if (!validation) {
        return ::media::Result<MediaGraph>::failure(validation.error());
    }

    MediaGraph graph;

    const MediaNodeId fileInput = graph.addNode(
        MediaNodeKind::FileInput,
        "local.file.input",
        "Local file input");
    const MediaNodeId demux = graph.addNode(
        MediaNodeKind::Demux,
        "local.demux",
        "Local demux");
    const MediaNodeId split = graph.addNode(
        MediaNodeKind::StreamSplit,
        "local.stream.split",
        "Local stream split");
    const MediaNodeId fileOutput = graph.addNode(
        MediaNodeKind::FileOutput,
        "local.file.output",
        "Local file output");
    const MediaNodeId mux = graph.addNode(
        MediaNodeKind::FileMux,
        "local.file.mux",
        "Local file mux");

    graph.setNodeOption(fileInput, "url", options.inputUrl);
    graph.setNodeOption(fileOutput, "url", options.outputUrl);
    if (!options.outputFormat.empty()) {
        graph.setNodeOption(fileOutput, "format", options.outputFormat);
    }

    graph.addOutputPort(fileInput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        false);
    graph.addInputPort(demux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addOutputPort(demux,
                        "packet",
                        MediaStreamKind::Any,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        true);
    graph.addInputPort(split,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.addOutputPort(fileOutput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        false);
    graph.addInputPort(mux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addInputPort(mux,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::EncodedPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.connect(fileInput,
                  "format",
                  demux,
                  "format",
                  "local.file.input.format -> local.demux.format",
                  blockingQueuePolicy(options.metadataQueueCapacity));
    graph.connect(demux,
                  "packet",
                  split,
                  "packet",
                  "local.demux.packet -> local.stream.split.packet",
                  blockingQueuePolicy(options.packetQueueCapacity));
    graph.connect(fileOutput,
                  "format",
                  mux,
                  "format",
                  "local.file.output.format -> local.file.mux.format",
                  blockingQueuePolicy(options.metadataQueueCapacity));

    if (options.includeVideo) {
        const MediaNodeId videoDecode = graph.addNode(
            MediaNodeKind::VideoDecode,
            "local.video.decode",
            "Local video decode");
        const MediaNodeId videoEncode = graph.addNode(
            MediaNodeKind::VideoEncode,
            "local.video.encode",
            "Local video encode");

        graph.addOutputPort(split,
                            "video",
                            MediaStreamKind::Video,
                            MediaEdgeKind::InputPacket,
                            MediaPayloadKind::Packet,
                            false,
                            true);
        graph.addInputPort(videoDecode,
                           "packet",
                           MediaStreamKind::Video,
                           MediaEdgeKind::InputPacket,
                           MediaPayloadKind::Packet,
                           true,
                           true);
        graph.addOutputPort(videoDecode,
                            "frame",
                            MediaStreamKind::Video,
                            MediaEdgeKind::RawFrame,
                            MediaPayloadKind::Frame,
                            true,
                            true);
        graph.addInputPort(videoEncode,
                           "frame",
                           MediaStreamKind::Video,
                           MediaEdgeKind::RawFrame,
                           MediaPayloadKind::Frame,
                           true,
                           true);
        graph.addOutputPort(videoEncode,
                            "packet",
                            MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket,
                            MediaPayloadKind::Packet,
                            true,
                            true);

        graph.connect(split,
                      "video",
                      videoDecode,
                      "packet",
                      "local.stream.split.video -> local.video.decode.packet",
                      blockingQueuePolicy(options.packetQueueCapacity));
        graph.connect(videoDecode,
                      "frame",
                      videoEncode,
                      "frame",
                      "local.video.decode.frame -> local.video.encode.frame",
                      blockingQueuePolicy(options.frameQueueCapacity));
        graph.connect(videoEncode,
                      "packet",
                      mux,
                      "packet",
                      "local.video.encode.packet -> local.file.mux.packet",
                      blockingQueuePolicy(options.muxQueueCapacity));
    }

    if (options.includeAudio) {
        const MediaNodeId audioCopy = graph.addNode(
            MediaNodeKind::AudioCopy,
            "local.audio.copy",
            "Local audio copy");

        graph.addOutputPort(split,
                            "audio",
                            MediaStreamKind::Audio,
                            MediaEdgeKind::InputPacket,
                            MediaPayloadKind::Packet,
                            false,
                            true);
        graph.addInputPort(audioCopy,
                           "packet",
                           MediaStreamKind::Audio,
                           MediaEdgeKind::InputPacket,
                           MediaPayloadKind::Packet,
                           true,
                           true);
        graph.addOutputPort(audioCopy,
                            "packet",
                            MediaStreamKind::Audio,
                            MediaEdgeKind::CopiedPacket,
                            MediaPayloadKind::Packet,
                            true,
                            true);

        graph.connect(split,
                      "audio",
                      audioCopy,
                      "packet",
                      "local.stream.split.audio -> local.audio.copy.packet",
                      blockingQueuePolicy(options.packetQueueCapacity));
        graph.connect(audioCopy,
                      "packet",
                      mux,
                      "packet",
                      "local.audio.copy.packet -> local.file.mux.packet",
                      blockingQueuePolicy(options.muxQueueCapacity));
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
