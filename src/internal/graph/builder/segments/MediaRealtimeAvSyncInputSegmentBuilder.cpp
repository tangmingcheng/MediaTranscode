#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

#include <string_view>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

using Support = MediaRealtimeAvSyncInputGraphSupport;
constexpr std::string_view Owner = "MediaRealtimeAvSyncInputSegmentBuilder";

struct SharedNodes final {
    MediaNodeId sourceClock;
    MediaNodeId videoGenerationGate;
    MediaNodeId audioGenerationGate;
    MediaNodeId videoCanonical;
    MediaNodeId audioCanonical;
    MediaNodeId coordinator;
    MediaNodeId startupClock;
    MediaNodeId epochBinder;
    MediaNodeId activationSequencer;
    MediaNodeId releaseExtractor;
};

::media::Result<SharedNodes> addSharedNodes(
    MediaGraph& graph,
    const std::string& prefix)
{
    auto sourceClock = Support::addNode(
        graph, MediaNodeKind::SourceClockStateFanout,
        prefix + ".source_clock", "Source clock state fanout");
    auto videoGenerationGate = Support::addNode(
        graph, MediaNodeKind::LockedPacketGate,
        prefix + ".video.generation_gate",
        "Shared video generation gate");
    auto audioGenerationGate = Support::addNode(
        graph, MediaNodeKind::LockedPacketGate,
        prefix + ".audio.generation_gate",
        "Shared audio generation gate");
    auto videoCanonical = Support::addNode(
        graph, MediaNodeKind::CanonicalInput,
        prefix + ".video.canonical_input", "Canonical video input");
    auto audioCanonical = Support::addNode(
        graph, MediaNodeKind::CanonicalInput,
        prefix + ".audio.canonical_input", "Canonical audio input");
    auto coordinator = Support::addNode(
        graph, MediaNodeKind::AvStartupCoordinator,
        prefix + ".startup.coordinator", "A/V startup coordinator");
    auto startupClock = Support::addNode(
        graph, MediaNodeKind::AvStartupClock,
        prefix + ".startup.clock", "A/V startup master-clock tick");
    auto epochBinder = Support::addNode(
        graph, MediaNodeKind::PlaybackEpochBinder,
        prefix + ".startup.epoch_binder", "Playback epoch binder");
    auto activationSequencer = Support::addNode(
        graph, MediaNodeKind::ActivatedStartupReleaseSequencer,
        prefix + ".startup.activation_sequencer",
        "Activated startup release sequencer");
    auto releaseExtractor = Support::addNode(
        graph, MediaNodeKind::AvBoundReleaseExtractor,
        prefix + ".startup.release_extractor", "Atomic A/V release extractor");
    if (!sourceClock || !videoGenerationGate || !audioGenerationGate ||
        !videoCanonical || !audioCanonical || !coordinator || !startupClock ||
        !epochBinder || !activationSequencer || !releaseExtractor) {
        return ::media::Result<SharedNodes>::failure(
            ::media::ErrorInfo::internalError(
                "Synchronized input segment failed to assemble shared nodes"));
    }
    return ::media::Result<SharedNodes>::success(SharedNodes{
        sourceClock.value(), videoGenerationGate.value(),
        audioGenerationGate.value(), videoCanonical.value(),
        audioCanonical.value(), coordinator.value(), startupClock.value(),
        epochBinder.value(), activationSequencer.value(),
        releaseExtractor.value()});
}

::media::Result<void> addSharedPorts(
    MediaGraph& graph,
    const SharedNodes& nodes,
    int videoStreamIndex,
    int audioStreamIndex,
    MediaEdgeKind videoEdgeKind,
    MediaEdgeKind audioEdgeKind)
{
    if (auto status = Support::addInput(
            graph, nodes.sourceClock, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return status;
    }
    for (const char* port : {"video", "audio", "startup"}) {
        if (auto status = Support::addOutput(
                graph, nodes.sourceClock, port, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent,
                true, false); !status) return status;
    }
    for (const auto [node, stream] : {
             std::pair{nodes.videoGenerationGate, MediaStreamKind::Video},
             std::pair{nodes.audioGenerationGate, MediaStreamKind::Audio}}) {
        if (auto status = Support::addInput(
                graph, node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) return status;
        if (auto status = Support::addInput(
                graph, node, "clock", MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return status;
        }
        if (auto status = Support::addOutput(
                graph, node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) return status;
    }
    for (const auto [node, stream] : {
             std::pair{nodes.videoCanonical, MediaStreamKind::Video},
             std::pair{nodes.audioCanonical, MediaStreamKind::Audio}}) {
        if (auto status = Support::addInput(
                graph, node, "in", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) return status;
        if (auto status = Support::addOutput(
                graph, node, "out", MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return status;
        }
    }
    for (const char* port : {"video", "audio", "clock"}) {
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
            MediaPayloadKind::Packet, false, true,
            MediaGraphBuildSupport::streamIndexDescriptor(
                MediaStreamKind::Video, videoStreamIndex));
        !status) return status;
    return MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
        graph, Owner, nodes.releaseExtractor, "audio",
        MediaStreamKind::Audio, audioEdgeKind,
        MediaPayloadKind::Packet, false, true,
        MediaGraphBuildSupport::streamIndexDescriptor(
            MediaStreamKind::Audio, audioStreamIndex));
}

::media::Result<void> configureSharedNodes(
    MediaGraph& graph,
    const SharedNodes& nodes,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureLockedPacketGate(
                graph, nodes.videoGenerationGate,
                MediaStreamKind::Video, plan); !status) {
        return status;
    }
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureLockedPacketGate(
                graph, nodes.audioGenerationGate,
                MediaStreamKind::Audio, plan); !status) {
        return status;
    }
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureCanonicalInput(
                graph, nodes.videoCanonical, MediaScheduledStream::Video,
                plan); !status) return status;
    if (auto status = MediaRealtimeAvSyncNodeConfigurator::
            configureCanonicalInput(
                graph, nodes.audioCanonical, MediaScheduledStream::Audio,
                plan); !status) return status;
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
    const SharedNodes& nodes,
    const MediaRealtimeAvSyncProtocolInputEndpoints& protocol,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& metadata = plan.edgePolicies.metadata;
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    const auto& atomicMetadata = plan.edgePolicies.atomicMetadata;
    const bool demuxClock = std::holds_alternative<
        MediaDemuxTimestampInputClockAssemblyPlan>(
        plan.assembly.inputClock);
    const auto& protocolClockPolicy =
        demuxClock ? atomicMetadata : metadata;
    const auto& protocolVideoPolicy =
        demuxClock ? plan.edgePolicies.atomicVideoPacket : packet;
    const auto& protocolAudioPolicy =
        demuxClock ? plan.edgePolicies.atomicAudioPacket : packet;
    if (auto status = Support::connect(
            graph, protocol.sourceClock, nodes.sourceClock, "clock",
            "protocol clock -> source clock fanout",
            protocolClockPolicy); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, nodes.sourceClock, "video", nodes.videoGenerationGate,
            "clock", "source clock -> video generation gate", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.sourceClock, "audio", nodes.audioGenerationGate,
            "clock", "source clock -> audio generation gate", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, protocol.video, nodes.videoGenerationGate, "packet",
            "protocol video -> generation gate",
            protocolVideoPolicy); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, protocol.audio, nodes.audioGenerationGate, "packet",
            "protocol audio -> generation gate",
            protocolAudioPolicy); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, nodes.videoGenerationGate, "packet",
            nodes.videoCanonical, "in",
            "generation-bound video -> canonical input", packet); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, nodes.audioGenerationGate, "packet",
            nodes.audioCanonical, "in",
            "generation-bound audio -> canonical input", packet); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, nodes.videoCanonical, "out", nodes.coordinator,
            "video", "canonical video -> startup coordinator", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.audioCanonical, "out", nodes.coordinator,
            "audio", "canonical audio -> startup coordinator", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.sourceClock, "startup", nodes.startupClock,
            "clock", "source clock -> startup clock", metadata); !status) {
        return status;
    }
    if (auto status = Support::connect(
            graph, nodes.startupClock, "tick", nodes.coordinator,
            "clock", "master clock tick -> startup coordinator", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.coordinator, "release", nodes.epochBinder,
            "release", "startup release -> epoch binder", metadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.epochBinder, "transaction",
            nodes.activationSequencer, "transaction",
            "epoch transaction -> activation sequencer", atomicMetadata);
        !status) return status;
    if (auto status = Support::connect(
            graph, nodes.epochBinder, "preparation",
            nodes.releaseExtractor, "preparation",
            "epoch transaction -> video preparation extractor",
            atomicMetadata);
        !status) return status;
    return Support::connect(
        graph, nodes.activationSequencer, "bound_release",
        nodes.releaseExtractor, "bound_release",
        "activated release -> atomic extractor", atomicMetadata);
}

} // namespace

::media::Result<MediaRealtimeAvSyncInputEndpoints>
MediaRealtimeAvSyncInputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (options.prefix.empty() || !options.sources.videoPacket.valid() ||
        !options.sources.audioPacket.valid() ||
        options.releasedVideoStreamIndex < 0 ||
        options.releasedAudioStreamIndex < 0 ||
        (options.releasedVideoEdgeKind != MediaEdgeKind::InputPacket &&
         options.releasedVideoEdgeKind != MediaEdgeKind::EncodedPacket) ||
        options.releasedAudioEdgeKind != MediaEdgeKind::InputPacket ||
        !plan.groupKey.valid() ||
        !MediaAvSyncPlanValidator::validate(plan.synchronization)) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input segment requires complete planned inputs"));
    }
    if (!MediaAtomicOutputPolicyContract::accepts(
            plan.edgePolicies.atomicMetadata) ||
        !MediaAtomicOutputPolicyContract::accepts(
            plan.edgePolicies.atomicVideoPacket) ||
        !MediaAtomicOutputPolicyContract::accepts(
            plan.edgePolicies.atomicAudioPacket)) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
            "Synchronized input release requires a complete planned atomic output policy"));
    }
    const bool demuxClock = std::holds_alternative<
        MediaDemuxTimestampInputClockAssemblyPlan>(
        plan.assembly.inputClock);
    if (!demuxClock && !options.sources.protocolClock.valid()) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized protocol input requires its planned clock endpoint"));
    }
    auto protocol = MediaRealtimeAvSyncProtocolInputBuilder::build(
        graph, options, plan);
    if (!protocol) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            protocol.error());
    }
    auto nodes = addSharedNodes(graph, options.prefix);
    if (!nodes) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            nodes.error());
    }
    if (auto status = addSharedPorts(
            graph, nodes.value(),
            options.releasedVideoStreamIndex,
            options.releasedAudioStreamIndex,
            options.releasedVideoEdgeKind,
            options.releasedAudioEdgeKind); !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    if (auto status = configureSharedNodes(graph, nodes.value(), plan);
        !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    if (auto status = connectSharedTopology(
            graph, nodes.value(), protocol.value(), plan); !status) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            status.error());
    }
    return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::success(
        MediaRealtimeAvSyncInputEndpoints{
            MediaEndpoint{nodes.value().releaseExtractor, "video"},
            MediaEndpoint{nodes.value().releaseExtractor, "audio"},
            MediaEndpoint{nodes.value().activationSequencer, "activated"}});
}

} // namespace media::ffmpeg::graph
