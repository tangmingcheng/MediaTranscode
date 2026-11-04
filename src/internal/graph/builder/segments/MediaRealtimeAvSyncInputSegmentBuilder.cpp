#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"

#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using Support = MediaRealtimeAvSyncInputGraphSupport;
constexpr std::string_view Owner = "MediaRealtimeAvSyncInputSegmentBuilder";

::media::Result<MediaAvSyncSharedNodeBindings> addSharedNodes(
    MediaGraph& graph,
    const std::string& prefix,
    const MediaSourceInputNodeShape& declaration)
{
    const auto& route = declaration.startup;
    const bool hasAudio = declaration.has(MediaSourceInputRole::AudioGate);
    const bool decodedVideo = route.readiness == MediaVideoStartupReadiness::DecodedPresentationSpan;
    auto sourceClock = Support::addNode(
        graph, *declaration.ownedKind(MediaSourceInputRole::SourceClock),
        prefix + ".source_clock", "Source clock state fanout");
    auto videoGenerationGate = Support::addNode(
        graph, *declaration.ownedKind(MediaSourceInputRole::VideoGate),
        prefix + ".video.generation_gate",
        "Shared video generation gate");
    std::optional<MediaNodeId> audioGenerationGate;
    if (hasAudio) {
        auto added = Support::addNode(graph, *declaration.ownedKind(MediaSourceInputRole::AudioGate),
            prefix + ".audio.generation_gate", "Shared audio generation gate");
        if (!added) return ::media::Result<MediaAvSyncSharedNodeBindings>::failure(added.error());
        audioGenerationGate = added.value();
    }
    auto videoStartup = Support::addNode(
        graph, route.nodeKind(MediaStartupRouteRole::VideoInput),
        prefix + (decodedVideo ? ".video.decoded_startup" : ".video.canonical_input"), "Video startup input");
    std::optional<MediaNodeId> audioCanonical;
    if (hasAudio) {
        auto added = Support::addNode(graph, route.nodeKind(MediaStartupRouteRole::AudioInput),
            prefix + ".audio.canonical_input", "Canonical audio input");
        if (!added) return ::media::Result<MediaAvSyncSharedNodeBindings>::failure(added.error());
        audioCanonical = added.value();
    }
    auto coordinator = Support::addNode(
        graph, route.nodeKind(MediaStartupRouteRole::Coordinator),
        prefix + ".startup.coordinator", "A/V startup coordinator");
    auto startupClock = Support::addNode(
        graph, *declaration.ownedKind(MediaSourceInputRole::StartupClock),
        prefix + ".startup.clock", "A/V startup master-clock tick");
    auto epochBinder = Support::addNode(
        graph, route.nodeKind(MediaStartupRouteRole::Binder),
        prefix + ".startup.epoch_binder", "Playback epoch binder");
    auto activationSequencer = Support::addNode(
        graph, route.nodeKind(MediaStartupRouteRole::Sequencer),
        prefix + ".startup.activation_sequencer",
        "Activated startup release sequencer");
    auto releaseExtractor = Support::addNode(
        graph, route.nodeKind(MediaStartupRouteRole::Extractor),
        prefix + ".startup.release_extractor", "Atomic A/V release extractor");
    if (!sourceClock || !videoGenerationGate ||
        !videoStartup || !coordinator || !startupClock ||
        !epochBinder || !activationSequencer || !releaseExtractor) {
        return ::media::Result<MediaAvSyncSharedNodeBindings>::failure(
            ::media::ErrorInfo::internalError(
                "Synchronized input segment failed to assemble shared nodes"));
    }
    return ::media::Result<MediaAvSyncSharedNodeBindings>::success(MediaAvSyncSharedNodeBindings{
        sourceClock.value(), videoGenerationGate.value(),
        audioGenerationGate, videoStartup.value(),
        audioCanonical, coordinator.value(), startupClock.value(),
        epochBinder.value(), activationSequencer.value(),
        releaseExtractor.value()});
}

::media::Result<void> addSharedPorts(
    MediaGraph& graph,
    const MediaAvSyncSharedNodeBindings& nodes,
    int videoStreamIndex,
    int audioStreamIndex,
    MediaEdgeKind videoEdgeKind,
    MediaEdgeKind audioEdgeKind, bool decodedVideo)
{
    if (auto status = Support::addInput(
            graph, nodes.sourceClock, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    for (const char* port : {"video", "audio", "startup"}) {
        if (!nodes.audioGenerationGate && std::string_view(port) == "audio") continue;
        if (auto status = Support::addOutput(
                graph, nodes.sourceClock, port, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent,
                true, false); !status) return status;
    }
    for (const auto [node, stream] : {
             std::pair{std::optional<MediaNodeId>(nodes.videoGenerationGate), MediaStreamKind::Video},
             std::pair{nodes.audioGenerationGate, MediaStreamKind::Audio}}) {
        if (!node) continue;
        if (auto status = Support::addInput(
                graph, *node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) return status;
        if (auto status = Support::addInput(
                graph, *node, "clock", MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return status;
        }
        if (auto status = MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
                graph, Owner, *node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet, true, false,
                MediaGraphBuildSupport::streamIndexDescriptor(stream,
                    stream == MediaStreamKind::Video ? videoStreamIndex : audioStreamIndex)); !status) return status;
    }
    for (const auto [node, stream] : {
             std::pair{std::optional<MediaNodeId>(nodes.videoStartup), MediaStreamKind::Video},
             std::pair{nodes.audioCanonical, MediaStreamKind::Audio}}) {
        if (!node) continue;
        if (auto status = Support::addInput(
                graph, *node, decodedVideo && stream == MediaStreamKind::Video ? "frame" : "in", stream,
                decodedVideo && stream == MediaStreamKind::Video ? MediaEdgeKind::RawFrame : MediaEdgeKind::InputPacket,
                decodedVideo && stream == MediaStreamKind::Video ? MediaPayloadKind::Frame : MediaPayloadKind::Packet); !status) return status;
        if (auto status = Support::addOutput(
                graph, *node, "out", MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return status;
        }
    }
    for (const char* port : {"video", "audio", "clock"}) {
        if (!nodes.audioCanonical && std::string_view(port) == "audio") continue;
        if (auto status = Support::addInput(
                graph, nodes.coordinator, port, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return status;
        }
    }
    if (auto status = Support::addOutput(
            graph, nodes.coordinator, "release", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addInput(
            graph, nodes.startupClock, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addOutput(
            graph, nodes.startupClock, "tick", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addInput(
            graph, nodes.epochBinder, "release", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addOutput(
            graph, nodes.epochBinder, "transaction", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addOutput(
            graph, nodes.epochBinder, "preparation", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addInput(
            graph, nodes.activationSequencer, "transaction",
            MediaStreamKind::Metadata, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent); !status) return status;
    if (auto status = Support::addOutput(
            graph, nodes.activationSequencer, "activated",
            MediaStreamKind::Metadata, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent, false, true); !status) return status;
    if (auto status = Support::addOutput(
            graph, nodes.activationSequencer, "bound_release",
            MediaStreamKind::Metadata, MediaEdgeKind::Event,
            MediaPayloadKind::GraphEvent); !status) return status;
    if (auto status = Support::addInput(
            graph, nodes.releaseExtractor, "preparation", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = Support::addInput(
            graph, nodes.releaseExtractor, "bound_release", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
            graph, Owner, nodes.releaseExtractor, "video",
            MediaStreamKind::Video, videoEdgeKind,
            decodedVideo ? MediaPayloadKind::Frame : MediaPayloadKind::Packet, false, true,
            MediaGraphBuildSupport::streamIndexDescriptor(
                MediaStreamKind::Video, videoStreamIndex));
        !status) return status;
    if (!nodes.audioCanonical) return ::media::Result<void>::success();
    return MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
        graph, Owner, nodes.releaseExtractor, "audio",
        MediaStreamKind::Audio, audioEdgeKind,
        MediaPayloadKind::Packet, false, true,
        MediaGraphBuildSupport::streamIndexDescriptor(
            MediaStreamKind::Audio, audioStreamIndex));
}

::media::Result<void> configureSharedNodes(
    MediaGraph& graph,
    const MediaAvSyncSharedNodeBindings& nodes,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureLockedPacketGate(
                graph, nodes.videoGenerationGate,
                MediaStreamKind::Video, plan); !status) {
        return status;
    }
    if (nodes.audioGenerationGate) {
        if (auto status = MediaRealtimeAvSyncNodeConfigurator::
                configureLockedPacketGate(
                    graph, *nodes.audioGenerationGate,
                    MediaStreamKind::Audio, plan); !status) {
            return status;
        }
    }
    if (std::holds_alternative<MediaDecodedVideoAssemblyTiming>(plan.assembly.video.timing)) {
        if (auto status = Support::setOption(graph, nodes.videoStartup,
                "decoded_video_startup.source_identity", plan.assembly.video.sourceIdentity); !status) return status;
        if (auto status = Support::setOption(graph, nodes.videoStartup,
                "decoded_video_startup.initial_generation", std::to_string(plan.assembly.initialGeneration)); !status) return status;
    } else if (auto status = MediaRealtimeAvSyncNodeConfigurator::configureCanonicalInput(
            graph, nodes.videoStartup, MediaScheduledStream::Video, plan); !status) return status;
    if (nodes.audioCanonical) {
        if (auto status = MediaRealtimeAvSyncNodeConfigurator::
                configureCanonicalInput(
                    graph, *nodes.audioCanonical, MediaScheduledStream::Audio,
                    plan); !status) return status;
    }
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureStartupCoordinator(graph, nodes.coordinator, plan);
        !status) return status;
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureStartupClock(graph, nodes.startupClock, plan);
        !status) return status;
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configurePlaybackEpochBinder(graph, nodes.epochBinder, plan);
        !status) return status;
    if (auto status =
            MediaRealtimeAvSyncNodeConfigurator::configureActivationSequencer(
                graph, nodes.activationSequencer, plan);
        !status) {
        return status;
    }
    return MediaRealtimeAvSyncNodeConfigurator::configureBoundReleaseExtractor(
        graph, nodes.releaseExtractor, plan);
}

::media::Result<void> connectSharedTopology(
    MediaGraph& graph,
    const MediaAvSyncInputBindings& bindings,
    const MediaRealtimeAvSourceRuntimePlan& plan,
    const MediaStartupRoutePlan& route)
{
    if (!plan.synchronization.sourceClockMode)
        return ::media::Result<void>::failure(::media::ErrorInfo::notInitialized(
            "Shared synchronized input requires its planned source clock mode"));
    auto connections = MediaSourceInputConnectionsPlanner::plan(route.shape.members,
        route.shape.readiness, *plan.synchronization.sourceClockMode, plan.edgePolicies);
    if (!connections) return ::media::Result<void>::failure(connections.error());
    for (const auto& connection : connections.value()) {
        if (connection.phase != MediaSourceInputConnectionPhase::Shared) continue;
        auto from = bindings.endpoint(connection.from, {});
        const auto to = bindings.endpoint(connection.to, {});
        if (!connection.output.empty()) from.port = connection.output;
        if (auto status = Support::connect(graph, from, to.node, std::string(connection.input),
                std::string(connection.label), connection.policy); !status) return status;
    }
    const auto connectRoute = [&](const MediaStartupRouteConnection& connection) -> ::media::Result<void> {
        const auto from = bindings.shared.routeNode(connection.from);
        const auto to = bindings.shared.routeNode(connection.to);
        if (!from || !to) return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Startup route references a missing assembled role"));
        return Support::connect(graph, *from, std::string(connection.output), *to,
            std::string(connection.input), std::string(connection.label), route.policy(connection));
    };
    for (const auto& connection : route.shape.connections()) {
        if (!route.shape.includes(connection) || connection.to != MediaStartupRouteRole::Coordinator) continue;
        if (auto status = connectRoute(connection); !status) return status;
    }
    for (const auto& connection : route.shape.connections()) {
        if (!route.shape.includes(connection) || connection.to == MediaStartupRouteRole::Coordinator) continue;
        if (auto status = connectRoute(connection); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<MediaRealtimeAvSyncInputEndpoints>
MediaRealtimeAvSyncInputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    const bool hasAudio = plan.synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    const bool decodedVideo = std::holds_alternative<MediaDecodedVideoAssemblyTiming>(plan.assembly.video.timing);
    if (decodedVideo != options.decodedVideo.has_value() ||
        decodedVideo != (plan.synchronization.startup.videoReadiness ==
            MediaVideoStartupReadiness::DecodedPresentationSpan) ||
        (decodedVideo && (std::holds_alternative<MediaObservedVideoCadence>(options.decodedVideo->timing) ||
            plan.synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution ||
            plan.synchronization.sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports ||
            options.decodedVideo->sourceStreamIndex != options.releasedVideoStreamIndex)))
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("Decoded startup requires its source decoder plan and frame release"));
    if (options.prefix.empty() || !options.sources.videoPacket.valid() ||
        options.sources.audioPacket.valid() != hasAudio ||
        options.releasedVideoStreamIndex < 0 ||
        (hasAudio ? options.releasedAudioStreamIndex < 0 :
            options.releasedAudioStreamIndex != invalidMediaStreamIndex) ||
        (decodedVideo ? options.releasedVideoEdgeKind != MediaEdgeKind::RawFrame :
            (options.releasedVideoEdgeKind != MediaEdgeKind::InputPacket &&
             options.releasedVideoEdgeKind != MediaEdgeKind::EncodedPacket)) ||
        (hasAudio ? options.releasedAudioEdgeKind !=
            (plan.audioPipeline.branchMode == MediaBranchMode::CopyPacket
                 ? MediaEdgeKind::EncodedPacket : MediaEdgeKind::InputPacket)
            : options.releasedAudioEdgeKind != MediaEdgeKind::Unknown) ||
        !plan.groupKey.valid() ||
        (plan.synchronization.domainRole != MediaAvSyncDomainRole::SharedSourceOutput &&
         plan.synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution) ||
        !MediaAvSyncPlanValidator::validateRuntime(plan.synchronization)) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input segment requires complete planned inputs"));
    }
    if (!MediaAtomicOutputPolicyContract::accepts(
            plan.edgePolicies.atomicMetadata) ||
        !MediaAtomicOutputPolicyContract::accepts(
            decodedVideo ? plan.edgePolicies.preparedVideoFrame : plan.edgePolicies.atomicVideoPacket) ||
        (hasAudio && !MediaAtomicOutputPolicyContract::accepts(
            plan.edgePolicies.atomicAudioPacket))) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
            "Synchronized input release requires a complete planned atomic output policy"));
    }
    if (!plan.synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::notInitialized(
                "Synchronized input requires its planner-selected source clock mode"));
    }
    const bool demuxClock =
        *plan.synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::DemuxTimestamps;
    if (!demuxClock && !options.sources.protocolClock.valid()) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized protocol input requires its planned clock endpoint"));
    }
    auto routeShape = MediaStartupRoutePlanner::describe(*plan.synchronization.members,
        *plan.synchronization.startup.videoReadiness);
    if (!routeShape) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(routeShape.error());
    auto route = MediaStartupRoutePlanner::bind(routeShape.value(),
        plan.edgePolicies.metadata, plan.edgePolicies.atomicMetadata);
    if (!route) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(route.error());
    auto protocol = MediaRealtimeAvSyncProtocolInputBuilder::build(
        graph, options, plan);
    if (!protocol) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            protocol.error());
    }
    auto nodes = addSharedNodes(graph, options.prefix,
        {route.value().shape, *plan.synchronization.sourceClockMode});
    if (!nodes) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            nodes.error());
    }
    if (auto status = addSharedPorts(
            graph, nodes.value(),
            options.releasedVideoStreamIndex,
            options.releasedAudioStreamIndex,
            options.releasedVideoEdgeKind,
            options.releasedAudioEdgeKind, decodedVideo); !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    if (auto status = configureSharedNodes(graph, nodes.value(), plan);
        !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    MediaAvSyncInputBindings bindings{nodes.value(), std::move(protocol).value()};
    if (auto status = connectSharedTopology(
            graph, bindings, plan, route.value()); !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    auto sourceMembers = bindings.protocol.sourceMembers;
    std::optional<MediaVideoBranchBindings> decodedVideoBindings;
    if (decodedVideo) {
        auto decoderOptions = *options.decodedVideo;
        decoderOptions.packetSourceNode = nodes.value().videoGenerationGate;
        decoderOptions.packetSourcePort = "packet";
        auto decoder = MediaVideoTranscodeBranchBuilder::buildSourceDecoder(
            graph, decoderOptions, MediaVideoDecodeLineageProduct::SourceAccessUnit);
        if (!decoder) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(decoder.error());
        auto connections = MediaSourceInputConnectionsPlanner::plan(route.value().shape.members,
            route.value().shape.readiness, *plan.synchronization.sourceClockMode, plan.edgePolicies);
        if (!connections) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(connections.error());
        for (const auto& connection : connections.value()) {
            if (connection.phase != MediaSourceInputConnectionPhase::DecodedFrame) continue;
            auto from = bindings.endpoint(connection.from, decoder.value().frame);
            const auto to = bindings.endpoint(connection.to, decoder.value().frame);
            if (!connection.output.empty()) from.port = connection.output;
            if (auto status = Support::connect(graph, from, to.node,
                    std::string(connection.input), std::string(connection.label), connection.policy); !status)
                return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
        }
        decodedVideoBindings = std::move(decoder.value().bindings);
        sourceMembers.insert(sourceMembers.end(), decoder.value().processing.source.begin(),
            decoder.value().processing.source.end());
    }
    const auto& shared = nodes.value();
    sourceMembers.insert(sourceMembers.end(), {
        shared.sourceClock, shared.videoGenerationGate,
        shared.videoStartup, shared.coordinator,
        shared.startupClock, shared.epochBinder, shared.activationSequencer,
        shared.releaseExtractor});
    if (shared.audioGenerationGate) sourceMembers.push_back(*shared.audioGenerationGate);
    if (shared.audioCanonical) sourceMembers.push_back(*shared.audioCanonical);
    return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::success(
        MediaRealtimeAvSyncInputEndpoints{
            MediaEndpoint{nodes.value().releaseExtractor, "video"},
            hasAudio ? MediaEndpoint{nodes.value().releaseExtractor, "audio"} : MediaEndpoint{},
            MediaEndpoint{nodes.value().activationSequencer, "activated"},
            MediaAvRuntimeInputRegistration{
                nodes.value().epochBinder, nodes.value().activationSequencer,
                nodes.value().releaseExtractor, bindings.protocol.demuxClock,
                bindings.protocol.rtp ? std::optional{bindings.protocol.rtp->registered} : std::nullopt},
            std::move(sourceMembers), std::move(decodedVideoBindings), std::move(bindings)});
}

} // namespace media::ffmpeg::graph
