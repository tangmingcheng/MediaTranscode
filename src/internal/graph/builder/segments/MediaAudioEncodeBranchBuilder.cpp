#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include "internal/graph/builder/MediaAudioPlanOptionApplier.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchNodes.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

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
            options.syncGroup) {
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
        !options.correctionLookaheadWindows || !options.syncGroup ||
        !options.syncGroup->valid() ||
        *options.correctionLookaheadWindows == 0 ||
        *options.correctionLookaheadWindows >
            MediaAudioDriftServoLimits::MaximumCorrectionLookaheadWindows ||
        !options.lineageMode ||
        *options.lineageMode !=
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "external audio correction requires planned generation, lookahead, sync group, and synchronized lineage"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateLineageOptions(
    const MediaAudioEncodeBranchOptions& options)
{
    if (!options.lineageMode) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioEncodeBranchBuilder requires explicit audio lineage mode"));
    }
    if (*options.lineageMode ==
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (!options.lineageCapacity || *options.lineageCapacity == 0 ||
            !options.correctionMode ||
            *options.correctionMode !=
                MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "synchronized audio lineage requires positive planned capacity and external correction"));
        }
        return ::media::Result<void>::success();
    }
    if (*options.lineageMode != MediaAudioLineageExecutionMode::LegacyPlainPacket ||
        options.lineageCapacity ||
        !options.correctionMode ||
        *options.correctionMode != MediaAudioCorrectionExecutionMode::Disabled) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "legacy audio lineage rejects synchronized capacity"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applyLineageStageOptions(
    MediaGraph& graph,
    const MediaAudioEncodeBranchOptions& options,
    MediaNodeId node,
    std::string_view identity)
{
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, node, std::string(MediaAudioLineageModeOptionKey),
            std::string(mediaAudioLineageExecutionModeName(*options.lineageMode)));
        !status) {
        return status;
    }
    if (*options.lineageMode == MediaAudioLineageExecutionMode::LegacyPlainPacket) {
        return ::media::Result<void>::success();
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, node, "audio.lineage.identity", std::string(identity));
        !status) {
        return status;
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, owner, node, "audio.lineage.capacity",
        std::to_string(*options.lineageCapacity));
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
                                                 bool normalizePackets,
                                                 MediaAudioLineageExecutionMode lineageMode)
{
    MediaAudioEncodeBranchNodes nodes;
    if (normalizePackets) {
        nodes.packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize, prefix + ".packet_normalize", "Audio packet normalize");
    }
    nodes.codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, prefix + ".codec_resolver", "Audio codec resolver");
    nodes.decode = graph.addNode(MediaNodeKind::AudioDecode, prefix + ".decode", "Audio decode");
    if (lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        nodes.startupTrim = graph.addNode(
            MediaNodeKind::AudioStartupTrim, prefix + ".startup_trim",
            "Audio startup trim");
        nodes.driftController = graph.addNode(
            MediaNodeKind::AudioDriftController, prefix + ".drift_controller",
            "Audio drift controller");
    }
    nodes.resample = graph.addNode(MediaNodeKind::AudioResample, prefix + ".resample", "Audio resample");
    nodes.encode = graph.addNode(MediaNodeKind::AudioEncode, prefix + ".encode", "Audio encode");
    if (lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        nodes.canonicalizer = graph.addNode(
            MediaNodeKind::EncodedAudioCanonicalizer, prefix + ".canonicalizer",
            "Encoded audio canonicalizer");
    }
    return nodes;
}

::media::Result<void> addEncodePorts(MediaGraph& graph,
                                      const MediaAudioEncodeBranchOptions& options,
                                      const MediaAudioEncodeBranchNodes& nodes)
{
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

    if (nodes.startupTrim.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.startupTrim, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.startupTrim, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    }

    if (nodes.driftController.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, nodes.driftController, "audio", MediaStreamKind::Audio,
                MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                graph, owner, nodes.driftController, "correction", MediaStreamKind::Audio,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                graph, owner, nodes.driftController, "audio", MediaStreamKind::Audio,
                MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    }

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
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.encode, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (nodes.canonicalizer.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, nodes.canonicalizer, "encoded", MediaStreamKind::Audio,
                MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        return MediaGraphBuildSupport::addOutputPortChecked(
            graph, owner, nodes.canonicalizer, "canonical", MediaStreamKind::Audio,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    }
    return ::media::Result<void>::success();
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
    if (nodes.startupTrim.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.decode, "frame", nodes.startupTrim, "frame", options.prefix + ".decode.frame -> startup_trim.frame", policies.audioFrame); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.startupTrim, "frame", nodes.driftController, "audio", options.prefix + ".startup_trim.frame -> drift_controller.audio", policies.audioFrame); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.driftController, "audio", nodes.resample, "frame", options.prefix + ".drift_controller.audio -> resample.frame", policies.audioDriftTransaction); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.decode, "frame", nodes.resample, "frame", options.prefix + ".decode.frame -> resample.frame", policies.audioFrame); !status) return status;
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, nodes.driftController, "correction", nodes.resample,
                "correction", options.prefix +
                    ".drift_controller.correction -> resample.correction",
                policies.audioDriftTransaction); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.encode, "codec", options.prefix + ".codec_resolver.encoder -> encode.codec", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.resample, "frame", nodes.encode, "frame", options.prefix + ".resample.frame -> encode.frame", policies.audioFrame); !status) return status;
    if (nodes.canonicalizer.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, nodes.encode, "packet", nodes.canonicalizer, "encoded",
                options.prefix + ".encode.packet -> canonicalizer.encoded",
                policies.audioPacket); !status) return status;
        return ::media::Result<void>::success();
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<MediaEncodedBranchEndpoints> MediaAudioEncodeBranchBuilder::build(
    MediaGraph& graph,
    const MediaAudioEncodeBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioEncodeBranchBuilder requires transcode_frame audio branch"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires planned audio source stream index"));
    }
    if (!options.plan.resolvedOutput ||
        options.plan.resolvedOutput->branchMode() != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioEncodeBranchBuilder requires complete resolved audio output"));
    }
    if (!options.normalizePackets.has_value()) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires explicit packet normalization policy"));
    }
    if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(
            graph, owner,
            MediaEndpoint{options.packetSourceNode, options.packetSourcePort},
            MediaStreamKind::Audio, MediaEdgeKind::InputPacket,
            options.plan.sourceStreamIndex); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = validateLineageOptions(options); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = validateCorrectionOptions(options); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (*options.correctionMode ==
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
        !MediaAtomicOutputPolicyContract::accepts(
            options.edgePolicies.audioDriftTransaction)) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized audio correction requires a complete planned atomic output policy"));
    }

    const MediaAudioEncodeBranchNodes nodes = addAudioEncodeNodes(
        graph, options.prefix, *options.normalizePackets, *options.lineageMode);
    if (auto status = applyLineageStageOptions(
            graph, options, nodes.decode, MediaAudioDecodeLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = applyLineageStageOptions(
            graph, options, nodes.resample, MediaAudioResampleLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = applyLineageStageOptions(
            graph, options, nodes.encode, MediaAudioEncodeLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (nodes.startupTrim.isValid()) {
        if (auto status = applyLineageStageOptions(
                graph, options, nodes.startupTrim,
                MediaAudioStartupTrimLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (nodes.driftController.isValid()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.driftController,
                "audio_drift_controller.sync_group",
                options.syncGroup->value()); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = MediaAudioPlanOptionApplier::applySelectedPlan(
            graph, nodes, options.plan, *options.normalizePackets); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = applyCorrectionOptions(graph, options, nodes.resample); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = addEncodePorts(graph, options, nodes); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = connectEncodePorts(graph, options, nodes); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    return ::media::Result<MediaEncodedBranchEndpoints>::success({
        {nodes.encode, "codec"},
        nodes.canonicalizer.isValid()
            ? MediaEndpoint{nodes.canonicalizer, "canonical"}
            : MediaEndpoint{nodes.encode, "packet"}});
}

} // namespace media::ffmpeg::graph
