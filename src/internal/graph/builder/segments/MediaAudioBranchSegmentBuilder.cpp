#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioBranchSegmentBuilder";

::media::Result<void> setNodeOptionChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setAudioSourceStreamOption(MediaGraph& graph, MediaNodeId nodeId, int sourceStreamIndex)
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
    return MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodeId, name, streamKind, edgeKind, payloadKind, required, multiple);
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& name, MediaStreamKind streamKind, MediaEdgeKind edgeKind, MediaPayloadKind payloadKind, bool required, bool multiple)
{
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodeId, name, streamKind, edgeKind, payloadKind, required, multiple);
}

::media::Result<void> connectChecked(MediaGraph& graph, MediaNodeId fromNode, const std::string& fromPort, MediaNodeId toNode, const std::string& toPort, const std::string& label, std::size_t capacity)
{
    return MediaGraphBuildSupport::connectChecked(graph,
                                                  owner,
                                                  fromNode,
                                                  fromPort,
                                                  toNode,
                                                  toPort,
                                                  label,
                                                  MediaGraphBuildSupport::blockingQueuePolicy(capacity));
}

::media::Result<void> validateEndpoints(const MediaAudioBranchSegmentOptions& options)
{
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires format source endpoint"));
    }
    if (!options.packetSourceNode.isValid() || options.packetSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires packet source endpoint"));
    }
    if (!options.muxNode.isValid() || options.muxCodecPort.empty() || options.muxPacketPort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires mux endpoints"));
    }
    if (options.queues.metadata == 0 || options.queues.packet == 0 ||
        options.queues.frame == 0 || options.queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder queue capacities must be greater than 0"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> buildCopyBranch(MediaGraph& graph,
                                      const MediaAudioBranchSegmentOptions& options)
{
    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix + ".copy";
    branchOptions.streamKind = MediaStreamKind::Audio;
    branchOptions.sourceStreamIndex = options.plan.sourceStreamIndex;
    branchOptions.formatSourceNode = options.formatSourceNode;
    branchOptions.formatSourcePort = options.formatSourcePort;
    branchOptions.packetSourceNode = options.packetSourceNode;
    branchOptions.packetSourcePort = options.packetSourcePort;
    branchOptions.muxNode = options.muxNode;
    branchOptions.muxCodecPort = options.muxCodecPort;
    branchOptions.muxPacketPort = options.muxPacketPort;
    branchOptions.queues = options.queues;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

::media::Result<void> buildTranscodeBranch(MediaGraph& graph,
                                           const MediaAudioBranchSegmentOptions& options)
{
    const MediaGraphQueueParameters& queues = options.queues;
    const MediaAudioTranscodeParameters& audio = options.parameters;
    const int streamIndex = options.plan.sourceStreamIndex;
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize, options.prefix + ".packet_normalize", "Audio packet normalize");
    const MediaNodeId codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, options.prefix + ".codec_resolver", "Audio codec resolver");
    const MediaNodeId decode = graph.addNode(MediaNodeKind::AudioDecode, options.prefix + ".decode", "Audio decode");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, options.prefix + ".resample", "Audio resample");
    const MediaNodeId encode = graph.addNode(MediaNodeKind::AudioEncode, options.prefix + ".encode", "Audio encode");

    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph, owner, packetNormalize, MediaStreamKind::Audio, streamIndex); !status) return status;
    if (auto status = setAudioSourceStreamOption(graph, codecResolver, streamIndex); !status) return status;
    if (auto status = applyAudioEncodeOptions(graph, codecResolver, audio, options.plan); !status) return status;

    const MediaPortId audioPort = graph.addOutputPort(options.packetSourceNode, options.packetSourcePort, MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(audioPort, owner, options.packetSourcePort); !status) return status;
    graph.setPortFormatDescriptor(audioPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, streamIndex));

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

    if (auto status = connectChecked(graph, options.formatSourceNode, options.formatSourcePort, packetNormalize, "format", options.prefix + ".format -> packet_normalize.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, options.packetSourceNode, options.packetSourcePort, packetNormalize, "packet", options.prefix + ".packet -> packet_normalize.packet", queues.packet); !status) return status;
    if (auto status = connectChecked(graph, options.formatSourceNode, options.formatSourcePort, codecResolver, "format", options.prefix + ".format -> codec_resolver.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "decoder", decode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, packetNormalize, "packet", decode, "packet", options.prefix + ".packet_normalize.packet -> decode.packet", queues.packet); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "encoder", resample, "codec", options.prefix + ".codec_resolver.encoder -> resample.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, decode, "frame", resample, "frame", options.prefix + ".decode.frame -> resample.frame", queues.frame); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "encoder", encode, "codec", options.prefix + ".codec_resolver.encoder -> encode.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, resample, "frame", encode, "frame", options.prefix + ".resample.frame -> encode.frame", queues.frame); !status) return status;
    if (auto status = connectChecked(graph, encode, "codec", options.muxNode, options.muxCodecPort, options.prefix + ".encode.codec -> mux.codec", queues.metadata); !status) return status;
    return connectChecked(graph, encode, "packet", options.muxNode, options.muxPacketPort, options.prefix + ".encode.packet -> mux.packet", queues.mux);
}

} // namespace

::media::Result<bool> MediaAudioBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaAudioBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<bool>::success(false);
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires planned audio source stream index"));
    }
    if (auto status = validateEndpoints(options); !status) {
        return ::media::Result<bool>::failure(status.error());
    }

    ::media::Result<void> buildStatus = ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("MediaAudioBranchSegmentBuilder unsupported audio branch mode"));
    if (options.plan.branchMode == MediaBranchMode::CopyPacket) {
        buildStatus = buildCopyBranch(graph, options);
    } else if (options.plan.branchMode == MediaBranchMode::TranscodeFrame) {
        buildStatus = buildTranscodeBranch(graph, options);
    }

    if (!buildStatus) {
        return ::media::Result<bool>::failure(buildStatus.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
