#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include "internal/graph/builder/MediaAudioPlanOptionApplier.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchNodes.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioEncodeBranchBuilder";

::media::Result<void> validateCorrectionOptions(
    const MediaAudioEncodeBranchOptions& options)
{
    if (!options.correctionMode) {
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "MediaAudioEncodeBranchBuilder requires explicit audio correction mode"));
    }
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::Disabled) {
        if (options.correctionGeneration || options.correctionLookaheadWindows ||
            options.correctionSourceNode.isValid() ||
            !options.correctionSourcePort.empty()) {
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                "disabled audio correction rejects external correction configuration"));
        }
        return ::media::Result<void>::success();
    }
    if (*options.correctionMode !=
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "MediaAudioEncodeBranchBuilder rejects unknown audio correction mode"));
    }
    if (!options.correctionGeneration || *options.correctionGeneration == 0 ||
        !options.correctionLookaheadWindows ||
        *options.correctionLookaheadWindows == 0 ||
        *options.correctionLookaheadWindows >
            MediaAudioDriftServoLimits::MaximumCorrectionLookaheadWindows ||
        !options.correctionSourceNode.isValid() || options.correctionSourcePort.empty()) {
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "external audio correction requires generation and source endpoint"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applyCorrectionOptions(
    MediaGraph& graph,
    const MediaAudioEncodeBranchOptions& options,
    MediaNodeId resample)
{
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph,
            owner,
            resample,
            MediaAudioCorrectionOptionKey::Mode,
            mediaAudioCorrectionExecutionModeName(*options.correctionMode)); !status) {
        return status;
    }
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph,
            owner,
            resample,
            MediaAudioCorrectionOptionKey::Generation,
            std::to_string(*options.correctionGeneration)); !status) {
            return status;
        }
        return MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, resample,
            MediaAudioCorrectionOptionKey::LookaheadWindows,
            std::to_string(*options.correctionLookaheadWindows));
    }
    return ::media::Result<void>::success();
}

MediaAudioEncodeBranchNodes addAudioEncodeNodes(MediaGraph& graph,
                                                 const std::string& prefix,
                                                 bool normalizePackets)
{
    MediaAudioEncodeBranchNodes nodes;
    if (normalizePackets) {
        nodes.packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize, prefix + ".packet_normalize", "Audio packet normalize");
    }
    nodes.codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, prefix + ".codec_resolver", "Audio codec resolver");
    nodes.decode = graph.addNode(MediaNodeKind::AudioDecode, prefix + ".decode", "Audio decode");
    nodes.resample = graph.addNode(MediaNodeKind::AudioResample, prefix + ".resample", "Audio resample");
    nodes.encode = graph.addNode(MediaNodeKind::AudioEncode, prefix + ".encode", "Audio encode");
    return nodes;
}

::media::Result<void> addEncodePorts(MediaGraph& graph,
                                      const MediaAudioEncodeBranchOptions& options,
                                      const MediaAudioEncodeBranchNodes& nodes)
{
    const MediaPortId audioPort = graph.addOutputPort(options.packetSourceNode, options.packetSourcePort, MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(audioPort, owner, options.packetSourcePort); !status) return status;
    graph.setPortFormatDescriptor(audioPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, options.plan.sourceStreamIndex));

    if (*options.normalizePackets) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    }

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.decode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.decode, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.decode, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, nodes.resample, "correction", MediaStreamKind::Audio,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.encode, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.encode, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
}

::media::Result<void> connectEncodePorts(MediaGraph& graph,
                                          const MediaAudioEncodeBranchOptions& options,
                                          const MediaAudioEncodeBranchNodes& nodes)
{
    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    if (*options.normalizePackets) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.packetNormalize, "format", options.prefix + ".format -> packet_normalize.format", policies.metadata); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.packetNormalize, "packet", options.prefix + ".packet -> packet_normalize.packet", policies.audioPacket); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.decode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", policies.metadata); !status) return status;
    if (*options.normalizePackets) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.packetNormalize, "packet", nodes.decode, "packet", options.prefix + ".packet_normalize.packet -> decode.packet", policies.audioPacket); !status) return status;
    } else {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.decode, "packet", options.prefix + ".packet -> decode.packet", policies.audioPacket); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.resample, "codec", options.prefix + ".codec_resolver.encoder -> resample.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.decode, "frame", nodes.resample, "frame", options.prefix + ".decode.frame -> resample.frame", policies.frame); !status) return status;
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, options.correctionSourceNode, options.correctionSourcePort,
                nodes.resample, "correction",
                options.prefix + ".correction -> resample.correction", policies.metadata); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.encode, "codec", options.prefix + ".codec_resolver.encoder -> encode.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.resample, "frame", nodes.encode, "frame", options.prefix + ".resample.frame -> encode.frame", policies.frame); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.encode, "codec", options.muxNode, options.muxCodecPort, options.prefix + ".encode.codec -> mux.codec", policies.metadata); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, nodes.encode, "packet", options.muxNode, options.muxPacketPort, options.prefix + ".encode.packet -> mux.packet", policies.audioMux);
}

} // namespace

::media::Result<void> MediaAudioEncodeBranchBuilder::build(
    MediaGraph& graph,
    const MediaAudioEncodeBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioEncodeBranchBuilder requires transcode_frame audio branch"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires planned audio source stream index"));
    }
    if (!options.plan.resolvedOutput ||
        options.plan.resolvedOutput->branchMode() != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioEncodeBranchBuilder requires complete resolved audio output"));
    }
    if (!options.normalizePackets.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires explicit packet normalization policy"));
    }
    if (auto status = validateCorrectionOptions(options); !status) {
        return status;
    }

    const MediaAudioEncodeBranchNodes nodes = addAudioEncodeNodes(graph, options.prefix, *options.normalizePackets);
    if (auto status = MediaAudioPlanOptionApplier::applySelectedPlan(
            graph, nodes, options.plan, *options.normalizePackets); !status) return status;
    if (auto status = applyCorrectionOptions(graph, options, nodes.resample); !status) return status;
    if (auto status = addEncodePorts(graph, options, nodes); !status) return status;
    return connectEncodePorts(graph, options, nodes);
}

} // namespace media::ffmpeg::graph
