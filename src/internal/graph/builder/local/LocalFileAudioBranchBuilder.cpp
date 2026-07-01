#include "internal/graph/builder/local/LocalFileAudioBranchBuilder.h"

#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include <array>
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

::media::Result<void> requirePort(MediaPortId portId, const char* name)
{
    if (!portId.isValid()) {
        return ::media::Result<void>::failure(::media::ErrorInfo::internalError(std::string("LocalFileAudioBranchBuilder failed to add port: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> requireEdge(MediaEdgeId edgeId, const char* name)
{
    if (!edgeId.isValid()) {
        return ::media::Result<void>::failure(::media::ErrorInfo::internalError(std::string("LocalFileAudioBranchBuilder failed to connect edge: ") + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> setNodeOptionChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    if (!graph.setNodeOption(nodeId, key, value)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::internalError("LocalFileAudioBranchBuilder failed to set option: " + key));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> setSourceStreamOption(MediaGraph& graph, MediaNodeId nodeId, int sourceStreamIndex)
{
    return setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioSourceStreamIndex, std::to_string(sourceStreamIndex));
}

::media::Result<void> applyAudioEncodeOptions(MediaGraph& graph,
                                              MediaNodeId nodeId,
                                              const MediaAudioTranscodeParameters& audio,
                                              const MediaAudioPipelinePlan& plan)
{
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioCodec, plan.targetCodecName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::PlannedEncoder, plan.targetEncoderName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioRateControl, mediaRateControlModeName(audio.rateControl)); !status) return status;
    if (audio.bitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioBitrateKbps, std::to_string(*audio.bitrateKbps)); !status) return status;
    if (audio.minBitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioMinBitrateKbps, std::to_string(*audio.minBitrateKbps)); !status) return status;
    if (audio.maxBitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioMaxBitrateKbps, std::to_string(*audio.maxBitrateKbps)); !status) return status;
    if (audio.bufferSizeKbits) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioBufferSizeKbits, std::to_string(*audio.bufferSizeKbits)); !status) return status;
    if (audio.sampleRate) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioSampleRate, std::to_string(*audio.sampleRate)); !status) return status;
    if (audio.channels) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioChannels, std::to_string(*audio.channels)); !status) return status;
    if (audio.quality) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioQuality, std::to_string(*audio.quality)); !status) return status;
    if (!audio.preset.empty()) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioPreset, audio.preset); !status) return status;
    if (!audio.profile.empty()) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioProfile, audio.profile); !status) return status;
    return ::media::Result<void>::success();
}

::media::Result<void> addInputPortChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& name, MediaStreamKind streamKind, MediaEdgeKind edgeKind, MediaPayloadKind payloadKind, bool required, bool multiple)
{
    return requirePort(graph.addInputPort(nodeId, name, streamKind, edgeKind, payloadKind, required, multiple), name.c_str());
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& name, MediaStreamKind streamKind, MediaEdgeKind edgeKind, MediaPayloadKind payloadKind, bool required, bool multiple)
{
    return requirePort(graph.addOutputPort(nodeId, name, streamKind, edgeKind, payloadKind, required, multiple), name.c_str());
}

::media::Result<void> buildAudioCopyBranch(MediaGraph& graph,
                                           const LocalFileTranscodeOptions& options,
                                           MediaNodeId fileInput,
                                           MediaNodeId split,
                                           MediaNodeId mux,
                                           int streamIndex)
{
    const MediaGraphQueueParameters& queues = options.parameters.queues;
    const MediaNodeId sourceConfig = graph.addNode(MediaNodeKind::AudioSourceConfig, "local.audio.source_config", "Local audio source config");
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::AudioPacketNormalize, "local.audio.packet_normalize", "Local audio packet normalize");
    auto sourceStatus = setSourceStreamOption(graph, sourceConfig, streamIndex);
    if (!sourceStatus) return sourceStatus;
    auto normalizeStatus = setSourceStreamOption(graph, packetNormalize, streamIndex);
    if (!normalizeStatus) return normalizeStatus;

    if (auto status = addInputPortChecked(graph, sourceConfig, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = addOutputPortChecked(graph, sourceConfig, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecParameters, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    const MediaPortId audioPort = graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = requirePort(audioPort, "split.audio"); !status) return status;
    graph.setPortFormatDescriptor(audioPort, streamIndexDescriptor(MediaStreamKind::Audio, streamIndex));
    if (auto status = addInputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    const std::array<std::pair<MediaEdgeId, const char*>, 5> edges {{
        { graph.connect(fileInput, "format", sourceConfig, "format", "local.file.input.format -> local.audio.source_config.format", blockingQueuePolicy(queues.metadata)), "source_config.format" },
        { graph.connect(sourceConfig, "codec", mux, "codec", "local.audio.source_config.codec -> local.file.mux.codec", blockingQueuePolicy(queues.metadata)), "source_config.codec" },
        { graph.connect(fileInput, "format", packetNormalize, "format", "local.file.input.format -> local.audio.packet_normalize.format", blockingQueuePolicy(queues.metadata)), "packet_normalize.format" },
        { graph.connect(split, "audio", packetNormalize, "packet", "local.stream.split.audio -> local.audio.packet_normalize.packet", blockingQueuePolicy(queues.packet)), "packet_normalize.packet" },
        { graph.connect(packetNormalize, "packet", mux, "packet", "local.audio.packet_normalize.packet -> local.file.mux.packet", blockingQueuePolicy(queues.mux)), "packet_normalize.mux" },
    }};
    for (const auto& edge : edges) {
        if (auto status = requireEdge(edge.first, edge.second); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> buildAudioEncodeBranch(MediaGraph& graph,
                                             const LocalFileTranscodeOptions& options,
                                             MediaNodeId fileInput,
                                             MediaNodeId split,
                                             MediaNodeId mux,
                                             const MediaAudioPipelinePlan& plan)
{
    const MediaGraphQueueParameters& queues = options.parameters.queues;
    const MediaAudioTranscodeParameters& audio = options.parameters.audio;
    const int streamIndex = plan.sourceStreamIndex;
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::AudioPacketNormalize, "local.audio.packet_normalize", "Local audio packet normalize");
    const MediaNodeId codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, "local.audio.codec_resolver", "Local audio codec resolver");
    const MediaNodeId decode = graph.addNode(MediaNodeKind::AudioDecode, "local.audio.decode", "Local audio decode");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "local.audio.resample", "Local audio resample");
    const MediaNodeId encode = graph.addNode(MediaNodeKind::AudioEncode, "local.audio.encode", "Local audio encode");

    for (MediaNodeId nodeId : { packetNormalize, codecResolver }) {
        auto status = setSourceStreamOption(graph, nodeId, streamIndex);
        if (!status) return status;
    }
    if (auto status = applyAudioEncodeOptions(graph, codecResolver, audio, plan); !status) return status;

    const MediaPortId audioPort = graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = requirePort(audioPort, "split.audio"); !status) return status;
    graph.setPortFormatDescriptor(audioPort, streamIndexDescriptor(MediaStreamKind::Audio, streamIndex));

    if (auto status = addInputPortChecked(graph, packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = addOutputPortChecked(graph, codecResolver, "decoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, codecResolver, "encoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, decode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, decode, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, decode, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, encode, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addOutputPortChecked(graph, encode, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    const std::array<std::pair<MediaEdgeId, const char*>, 10> edges {{
        { graph.connect(fileInput, "format", packetNormalize, "format", "local.file.input.format -> local.audio.packet_normalize.format", blockingQueuePolicy(queues.metadata)), "packet_normalize.format" },
        { graph.connect(split, "audio", packetNormalize, "packet", "local.stream.split.audio -> local.audio.packet_normalize.packet", blockingQueuePolicy(queues.packet)), "packet_normalize.packet" },
        { graph.connect(fileInput, "format", codecResolver, "format", "local.file.input.format -> local.audio.codec_resolver.format", blockingQueuePolicy(queues.metadata)), "codec_resolver.format" },
        { graph.connect(codecResolver, "decoder", decode, "codec", "local.audio.codec_resolver.decoder -> local.audio.decode.codec", blockingQueuePolicy(queues.metadata)), "decode.codec" },
        { graph.connect(packetNormalize, "packet", decode, "packet", "local.audio.packet_normalize.packet -> local.audio.decode.packet", blockingQueuePolicy(queues.packet)), "decode.packet" },
        { graph.connect(codecResolver, "encoder", resample, "codec", "local.audio.codec_resolver.encoder -> local.audio.resample.codec", blockingQueuePolicy(queues.metadata)), "resample.codec" },
        { graph.connect(decode, "frame", resample, "frame", "local.audio.decode.frame -> local.audio.resample.frame", blockingQueuePolicy(queues.frame)), "resample.frame" },
        { graph.connect(codecResolver, "encoder", encode, "codec", "local.audio.codec_resolver.encoder -> local.audio.encode.codec", blockingQueuePolicy(queues.metadata)), "encode.codec" },
        { graph.connect(resample, "frame", encode, "frame", "local.audio.resample.frame -> local.audio.encode.frame", blockingQueuePolicy(queues.frame)), "encode.frame" },
        { graph.connect(encode, "codec", mux, "codec", "local.audio.encode.codec -> local.file.mux.codec", blockingQueuePolicy(queues.metadata)), "encode.mux_codec" },
    }};
    for (const auto& edge : edges) {
        if (auto status = requireEdge(edge.first, edge.second); !status) return status;
    }
    auto packetEdge = requireEdge(graph.connect(encode, "packet", mux, "packet", "local.audio.encode.packet -> local.file.mux.packet", blockingQueuePolicy(queues.mux)), "encode.mux_packet");
    if (!packetEdge) return packetEdge;
    return ::media::Result<void>::success();
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
        if (auto status = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectAudio, "0"); !status) {
            return ::media::Result<bool>::failure(status.error());
        }
        return ::media::Result<bool>::success(false);
    }

    if (auto muxStatus = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectAudio, "1"); !muxStatus) {
        return ::media::Result<bool>::failure(muxStatus.error());
    }
    auto status = plan.mode == MediaAudioPipelineMode::Encode
        ? buildAudioEncodeBranch(graph, options, fileInput, split, mux, plan)
        : buildAudioCopyBranch(graph, options, fileInput, split, mux, plan.sourceStreamIndex);
    if (!status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
