#include "internal/graph/builder/local/LocalFileAudioBranchBuilder.h"

#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include <string>
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

MediaFormatDescriptor streamIndexDescriptor(MediaStreamKind streamKind, int streamIndex)
{
    MediaFormatDescriptor descriptor;
    descriptor.streamKind = streamKind;
    descriptor.streamIndex = streamIndex;
    return descriptor;
}

void setSourceStreamOption(MediaGraph& graph, MediaNodeId nodeId, int sourceStreamIndex)
{
    graph.setNodeOption(nodeId, "audio.source_stream_index", std::to_string(sourceStreamIndex));
}

} // namespace

::media::Result<bool> LocalFileAudioBranchBuilder::buildIfPlanned(MediaGraph& graph,
                                                                  const LocalFileTranscodeOptions& options,
                                                                  MediaNodeId fileInput,
                                                                  MediaNodeId split,
                                                                  MediaNodeId mux)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildAudioPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<bool>::failure(plannerOptions.error());
    }

    auto planResult = MediaAudioPipelinePlanner::planFileAudio(options.inputUrl, std::move(plannerOptions).value());
    if (!planResult) {
        return ::media::Result<bool>::failure(planResult.error());
    }

    MediaAudioPipelinePlan plan = std::move(planResult).value();
    if (!plan.enabled) {
        graph.setNodeOption(mux, "mux.expect_audio", "0");
        return ::media::Result<bool>::success(false);
    }

    const int streamIndex = plan.sourceStreamIndex;
    const MediaNodeId sourceConfig = graph.addNode(MediaNodeKind::AudioSourceConfig,
                                                   "local.audio.source_config",
                                                   "Local audio source config");
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::AudioPacketNormalize,
                                                      "local.audio.packet_normalize",
                                                      "Local audio packet normalize");

    setSourceStreamOption(graph, sourceConfig, streamIndex);
    setSourceStreamOption(graph, packetNormalize, streamIndex);
    graph.setNodeOption(mux, "mux.expect_audio", "1");

    graph.addInputPort(sourceConfig, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    graph.addOutputPort(sourceConfig, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecParameters, true, false);

    graph.addInputPort(packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false);
    const MediaPortId audioPort = graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    graph.setPortFormatDescriptor(audioPort, streamIndexDescriptor(MediaStreamKind::Audio, streamIndex));
    graph.addInputPort(packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", sourceConfig, "format", "local.file.input.format -> local.audio.source_config.format", blockingQueuePolicy(options.metadataQueueCapacity));
    graph.connect(sourceConfig, "codec", mux, "codec", "local.audio.source_config.codec -> local.file.mux.codec", blockingQueuePolicy(options.metadataQueueCapacity));
    graph.connect(fileInput, "format", packetNormalize, "format", "local.file.input.format -> local.audio.packet_normalize.format", blockingQueuePolicy(options.metadataQueueCapacity));
    graph.connect(split, "audio", packetNormalize, "packet", "local.stream.split.audio -> local.audio.packet_normalize.packet", blockingQueuePolicy(options.packetQueueCapacity));
    graph.connect(packetNormalize, "packet", mux, "packet", "local.audio.packet_normalize.packet -> local.file.mux.packet", blockingQueuePolicy(options.muxQueueCapacity));

    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
