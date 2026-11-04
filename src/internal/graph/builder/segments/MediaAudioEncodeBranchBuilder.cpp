#include "internal/graph/builder/segments/MediaBranchConnectionBindings.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/planner/audio/MediaAudioBranchConnectionsPlanner.h"
#include "internal/graph/sync/MediaAudioDriftServoLimits.h"

#include "internal/graph/builder/MediaAudioPlanOptionApplier.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchNodes.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioEncodeBranchBuilder";

struct AudioSegmentOptions final {
    const std::string& prefix;
    const MediaResolvedAudioOutputPlan* outputPlan;
    const MediaRealtimeEdgePolicySet& edgePolicies;
    std::optional<MediaAudioLineageExecutionMode> lineageMode;
    std::optional<std::size_t> lineageCapacity;
    std::optional<MediaAudioEncoderFifoRetentionPlan> encoderFifoRetention;
    std::optional<MediaAvSyncGroupKey> syncGroup;
    const MediaAudioEncodeBranchOptions* source;
};

AudioSegmentOptions segmentOptions(const MediaAudioEncodeBranchOptions& options)
{
    return {options.prefix, options.plan.resolvedOutput ? &*options.plan.resolvedOutput : nullptr,
        options.edgePolicies, options.lineageMode, options.lineageCapacity,
        options.encoderFifoRetention, options.syncGroup, &options};
}

::media::Result<void> validateCorrectionOptions(
    const MediaAudioEncodeBranchOptions& options)
{
    if (!options.correctionMode) {
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "MediaAudioEncodeBranchBuilder requires explicit audio correction mode"));
    }
    if (*options.correctionMode == MediaAudioCorrectionExecutionMode::Disabled) {
        if (options.correctionGeneration || options.correctionLookaheadWindows || options.maximumResamplerOutputBlockSamples ||
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
        !options.correctionLookaheadWindows || !options.maximumResamplerOutputBlockSamples ||
        *options.maximumResamplerOutputBlockSamples <= 0 || !options.syncGroup ||
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
    const AudioSegmentOptions& options, bool sourceOnly)
{
    if (!options.lineageMode) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioEncodeBranchBuilder requires explicit audio lineage mode"));
    }
    if (*options.lineageMode ==
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if ((!sourceOnly && !options.encoderFifoRetention) || (sourceOnly && options.encoderFifoRetention) || !options.lineageCapacity || *options.lineageCapacity == 0 ||
            !options.syncGroup || !options.syncGroup->valid() ||
            (options.source && (!options.source->correctionMode ||
             *options.source->correctionMode != MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired))) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "synchronized audio lineage requires planned capacity, group and source correction when present"));
        }
        return ::media::Result<void>::success();
    }
    if (*options.lineageMode != MediaAudioLineageExecutionMode::LegacyPlainPacket ||
        options.encoderFifoRetention || options.lineageCapacity || options.syncGroup ||
        (options.source && (!options.source->correctionMode ||
         *options.source->correctionMode != MediaAudioCorrectionExecutionMode::Disabled))) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "legacy audio lineage rejects synchronized capacity"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applyLineageStageOptions(
    MediaGraph& graph,
    const AudioSegmentOptions& options,
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
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, resample, MediaAudioCorrectionOptionKey::MaximumOutputBlockSamples,
                std::to_string(*options.maximumResamplerOutputBlockSamples)); !status) return status;
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
                                                 MediaAudioLineageExecutionMode lineageMode,
                                                 bool sourceOnly, bool outputOnly)
{
    const bool synchronized = lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto declaration = outputOnly ? MediaAudioBranchConnectionsPlanner::outputEncoder(synchronized)
        : MediaAudioBranchConnectionsPlanner::source(normalizePackets, synchronized, false);
    using Role = MediaAudioBranchRole;
    declaration.present[static_cast<std::size_t>(Role::Encode)] = !sourceOnly;
    declaration.present[static_cast<std::size_t>(Role::Canonicalizer)] = !sourceOnly && synchronized;
    const auto add = [&](Role role, const char* suffix, const char* label) {
        return declaration.has(role) ? graph.addNode(*declaration.ownedKind(role), prefix + suffix, label)
                                     : MediaNodeId::invalid();
    };
    MediaAudioEncodeBranchNodes nodes;
    nodes.packetNormalize = add(Role::Normalize, ".packet_normalize", "Audio packet normalize");
    nodes.codecResolver = add(Role::CodecResolver, ".codec_resolver", "Audio codec resolver");
    nodes.decode = add(Role::Decode, ".decode", "Audio decode");
    nodes.startupTrim = add(Role::Trim, ".startup_trim", "Audio startup trim");
    nodes.driftController = add(Role::Drift, ".drift_controller", "Audio drift controller");
    nodes.resample = add(Role::Resample, ".resample", "Audio resample");
    nodes.encode = add(Role::Encode, ".encode", "Audio encode");
    nodes.canonicalizer = add(Role::Canonicalizer, ".canonicalizer", "Encoded audio canonicalizer");
    return nodes;
}

::media::Result<void> addEncodePorts(MediaGraph& graph,
                                      const AudioSegmentOptions& options,
                                      const MediaAudioEncodeBranchNodes& nodes)
{
    if (nodes.packetNormalize.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    }

    if (nodes.decode.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    }
    if (nodes.encode.isValid()) if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;

    if (nodes.decode.isValid()) {
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
        if (*options.source->correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired) {
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                    graph, owner, nodes.resample, "correction", MediaStreamKind::Audio,
                    MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true); !status) return status;
        }
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    }
    if (!nodes.encode.isValid()) return ::media::Result<void>::success();
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

::media::Result<MediaAudioBranchBindings> connectEncodePorts(MediaGraph& graph,
                                          const AudioSegmentOptions& options,
                                          const MediaAudioEncodeBranchNodes& nodes,
                                          MediaEndpoint outputCodec)
{
    using Role = MediaAudioBranchRole;
    std::array<MediaEndpoint, static_cast<std::size_t>(Role::Count)> endpoints{{
        options.source ? MediaEndpoint{options.source->formatSourceNode, options.source->formatSourcePort} : MediaEndpoint{},
        options.source ? MediaEndpoint{options.source->packetSourceNode, options.source->packetSourcePort} : MediaEndpoint{},
        outputCodec, {nodes.packetNormalize, ""}, {nodes.codecResolver, ""}, {nodes.decode, ""},
        {nodes.startupTrim, ""}, {nodes.driftController, ""}, {nodes.resample, ""},
        {nodes.encode, ""}, {nodes.canonicalizer, ""}}};
    MediaAudioBranchShape shape{{}, options.source &&
        options.source->correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired};
    for (std::size_t i = 0; i < endpoints.size(); ++i) shape.present[i] = endpoints[i].node.isValid();
    auto connections = MediaAudioBranchConnectionsPlanner::plan(shape, options.edgePolicies);
    if (!connections) return ::media::Result<MediaAudioBranchBindings>::failure(connections.error());
    MediaAudioBranchBindings bindings{std::move(endpoints)};
    if(auto status=bindings.connect(graph,connections.value(),options.prefix,owner);!status)
        return ::media::Result<MediaAudioBranchBindings>::failure(status.error());
    return ::media::Result<MediaAudioBranchBindings>::success(std::move(bindings));
}

} // namespace

static ::media::Result<MediaEncodedBranchEndpoints> buildAudioSegment(
    MediaGraph& graph,
    const AudioSegmentOptions& options,
    MediaAudioSourceBranchEndpoints* source, MediaOutputEncoderEndpoints* output,
    MediaEndpoint outputCodec)
{
    const bool sourceOnly = source != nullptr;
    const bool outputOnly = output != nullptr;
    if (options.source && options.source->plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioEncodeBranchBuilder requires transcode_frame audio branch"));
    }
    if (options.source && options.source->plan.sourceStreamIndex < 0) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires planned audio source stream index"));
    }
    if (options.prefix.empty() || !options.outputPlan ||
        options.outputPlan->branchMode() != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAudioEncodeBranchBuilder requires complete resolved audio output"));
    }
    if (options.source && !options.source->normalizePackets.has_value()) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeBranchBuilder requires explicit packet normalization policy"));
    }
    if (!outputOnly) if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(
            graph, owner,
            MediaEndpoint{options.source->packetSourceNode, options.source->packetSourcePort},
            MediaStreamKind::Audio, MediaEdgeKind::InputPacket,
            options.source->plan.sourceStreamIndex); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = validateLineageOptions(options, sourceOnly); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (options.source) if (auto status = validateCorrectionOptions(*options.source); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (options.source && *options.source->correctionMode ==
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired &&
        !MediaAtomicOutputPolicyContract::accepts(
            options.edgePolicies.audioDriftTransaction)) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized audio correction requires a complete planned atomic output policy"));
    }

    const MediaAudioEncodeBranchNodes nodes = addAudioEncodeNodes(
        graph, options.prefix, options.source && *options.source->normalizePackets,
        *options.lineageMode, sourceOnly, outputOnly);
    if (!sourceOnly) outputCodec = {nodes.codecResolver, "encoder"};
    if (sourceOnly || outputOnly) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver,
                "codec_resolver.mode", sourceOnly ? "source_decode" : "output_branch"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (nodes.decode.isValid()) if (auto status = applyLineageStageOptions(
            graph, options, nodes.decode, MediaAudioDecodeLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (nodes.resample.isValid()) if (auto status = applyLineageStageOptions(
            graph, options, nodes.resample, MediaAudioResampleLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (nodes.encode.isValid()) if (auto status = applyLineageStageOptions(
            graph, options, nodes.encode, MediaAudioEncodeLineageIdentity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (options.encoderFifoRetention && nodes.encode.isValid()) {
        const auto& limit = *options.encoderFifoRetention;
        for (const auto& item : {
                 std::pair{"audio.fifo.input_samples", std::to_string(limit.maximumInputSamples)},
                 std::pair{"audio.fifo.samples", std::to_string(limit.maximumSamples)},
                 std::pair{"audio.fifo.bytes", std::to_string(limit.maximumBytes)},
                 std::pair{"audio.fifo.fragments", std::to_string(limit.maximumFragments)},
                 std::pair{"audio.fifo.frame_samples", std::to_string(limit.frameSamples)}}) {
            auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.encode, item.first, item.second);
            if (!status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
    }
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
    if (nodes.canonicalizer.isValid()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.canonicalizer,
                "audio_canonicalizer.sync_group",
                options.syncGroup->value()); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    auto applied = options.source
        ? MediaAudioPlanOptionApplier::applySelectedPlan(
              graph, nodes, options.source->plan, nodes.packetNormalize.isValid())
        : MediaAudioPlanOptionApplier::applyOutputPlan(graph, nodes, *options.outputPlan);
    if (!applied) return ::media::Result<MediaEncodedBranchEndpoints>::failure(applied.error());
    if (nodes.resample.isValid()) if (auto status = applyCorrectionOptions(graph, *options.source, nodes.resample); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = addEncodePorts(graph, options, nodes); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    auto bindings = connectEncodePorts(graph, options, nodes, outputCodec);
    if (!bindings) return ::media::Result<MediaEncodedBranchEndpoints>::failure(bindings.error());
    if (sourceOnly) {
        const auto declaration = MediaAudioBranchConnectionsPlanner::source(*options.source->normalizePackets,
            *options.lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
            *options.source->correctionMode == MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired);
        if (auto status = bindings.value().validateDeclaration(graph, declaration); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if(outputOnly) {
        const auto declaration=MediaAudioBranchConnectionsPlanner::outputEncoder(
            *options.lineageMode==MediaAudioLineageExecutionMode::SynchronizedReleasedAudio);
        if(auto status=bindings.value().validateDeclaration(graph,declaration);!status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    MediaProcessingNodeOwnership processing;
    for (const auto id : {nodes.packetNormalize, nodes.codecResolver,
             nodes.decode, nodes.startupTrim, nodes.driftController, nodes.resample}) {
        if (!id.isValid()) continue;
        if (outputOnly && id == nodes.codecResolver) processing.output.push_back(id);
        else processing.source.push_back(id);
    }
    if (nodes.encode.isValid()) processing.output.push_back(nodes.encode);
    if (nodes.canonicalizer.isValid()) processing.output.push_back(nodes.canonicalizer);
    if (source) *source = {{nodes.resample, "frame"}, nodes.codecResolver, std::nullopt, processing, bindings.value()};
    if (output) {
        output->frameInput = {nodes.encode, "frame"};
        output->codec = outputCodec;
        output->codecResolver = nodes.codecResolver;
    }
    return ::media::Result<MediaEncodedBranchEndpoints>::success({
        {nodes.encode, "codec"},
        nodes.canonicalizer.isValid()
            ? MediaEndpoint{nodes.canonicalizer, "canonical"}
            : MediaEndpoint{nodes.encode, "packet"},
        std::nullopt, std::move(processing), std::move(bindings).value()});
}

::media::Result<MediaEncodedBranchEndpoints> MediaAudioEncodeBranchBuilder::build(
    MediaGraph& graph, const MediaAudioEncodeBranchOptions& options)
{
    return buildAudioSegment(graph, segmentOptions(options), nullptr, nullptr, {});
}

::media::Result<MediaAudioSourceBranchEndpoints> MediaAudioEncodeBranchBuilder::buildSource(
    MediaGraph& graph, const MediaAudioEncodeBranchOptions& options, MediaEndpoint outputEncoderCodec)
{
    if (!outputEncoderCodec.node.isValid())
        return ::media::Result<MediaAudioSourceBranchEndpoints>::failure(::media::ErrorInfo::invalidArgument(
            "Audio source segment requires the shared output encoder codec"));
    MediaAudioSourceBranchEndpoints source;
    auto result = buildAudioSegment(graph, segmentOptions(options), &source, nullptr, outputEncoderCodec);
    if (!result) return ::media::Result<MediaAudioSourceBranchEndpoints>::failure(result.error());
    return ::media::Result<MediaAudioSourceBranchEndpoints>::success(std::move(source));
}

::media::Result<MediaOutputEncoderEndpoints> MediaAudioEncodeBranchBuilder::buildOutputEncoder(
    MediaGraph& graph, const MediaAudioOutputEncoderOptions& options)
{
    MediaOutputEncoderEndpoints output;
    auto result = buildAudioSegment(graph,
        {options.prefix, &options.plan, options.edgePolicies, options.lineageMode,
         options.lineageCapacity, options.encoderFifoRetention, options.syncGroup, nullptr},
        nullptr, &output, {});
    if (!result) return ::media::Result<MediaOutputEncoderEndpoints>::failure(result.error());
    output.encoded = std::move(result).value();
    return ::media::Result<MediaOutputEncoderEndpoints>::success(std::move(output));
}

} // namespace media::ffmpeg::graph
