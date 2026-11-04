#include "internal/graph/builder/realtime/MediaCompositionGraphBindings.h"
#include "internal/graph/builder/realtime/MediaCompositionGraphTopologyValidator.h"
#include "internal/graph/builder/realtime/MediaCompositionSourceInputValidator.h"
#include "internal/graph/builder/segments/MediaBranchConnectionBindings.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/planner/realtime/MediaAvAggregateFrameCreditPlanner.h"
#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeInputGraphBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionAggregateConnectionsPlanner.h"

#include <algorithm>
#include <array>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeCompositionGraphBuilder";

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

bool sameAudioFrames(const MediaResolvedAudioOutputPlan& a,
                     const MediaResolvedAudioOutputPlan& b)
{
    return a.branchMode() == MediaBranchMode::TranscodeFrame &&
        b.branchMode() == MediaBranchMode::TranscodeFrame &&
        a.sampleRate() == b.sampleRate() && a.channels() == b.channels() &&
        a.channelLayout() == b.channelLayout() && a.sampleFormat() == b.sampleFormat() &&
        a.codecFrameSamples() == b.codecFrameSamples();
}

::media::Status validateOptions(const MediaRealtimeCompositionGraphOptions& options)
{
    if (auto status = MediaAvContinuousAggregatePlanValidator::validate(options.aggregate); !status) return status;
    if (options.sources.empty())
        return invalid("Composition requires planned sources and aggregate");
    const auto& aggregate = options.aggregate;
    const auto& output = options.outputRuntime;
    if (auto status = MediaAvSyncPlanValidator::validateDomain(
            output.synchronization, MediaAvSyncDomainRole::ContinuousOutput); !status) return status;
    if (!output.synchronization.startup.requireVideoKeyFrame.has_value())
        return invalid("Composition output requires its planned generation-start key-frame policy");
    auto activation = output.scheduling.activationLead(output.protocolOutput);
    if (!activation || activation.value() != output.activationOutputLead ||
        aggregate.preparationLead != output.activationOutputLead)
        return invalid("Composition activation differs from its encoded output scheduling contract");
    const auto& encoder = options.outputVideo.encoder.encoderOpenContract;
    if (aggregate.sources.size() != options.sources.size() ||
        aggregate.canvas.tiles.size() != options.sources.size() ||
        aggregate.audioSource >= options.sources.size() ||
        output.groupKey != aggregate.outputGroupKey || !output.groupKey.valid() ||
        !encoder || encoder->width != aggregate.canvas.width || encoder->height != aggregate.canvas.height ||
        encoder->frameRate != aggregate.videoFrameRate ||
        !sameAudioFrames(output.audioOutput, aggregate.audio))
        return invalid("Composition output contracts disagree with the aggregate canvas or audio grid");
    if (!MediaAtomicOutputPolicyContract::accepts(output.edgePolicies.atomicMetadata))
        return invalid("Composition activation requires the planned atomic metadata policy");
    auto expectedFifo = MediaAudioEncoderFifoRetentionPlan::create(
        aggregate.audio, aggregate.audio.codecFrameSamples());
    if (!expectedFifo || !output.encoderFifoRetention ||
        *output.encoderFifoRetention != expectedFifo.value())
        return invalid("Composition output FIFO differs from its aggregate audio block contract");
    std::unordered_set<std::string> groups{output.groupKey.value()};
    for (std::size_t i = 0; i < options.sources.size(); ++i) {
        const auto& source = options.sources[i];
        const auto* runtime = &source.runtime;
        if (auto status = MediaAvSyncPlanValidator::validateDomain(
                runtime->synchronization, MediaAvSyncDomainRole::SourceContribution); !status) return status;
        const auto& input = aggregate.sources[i];
        const bool hasAudio = runtime->synchronization.members == MediaTranscodeStreamSet::AudioVideo;
        if (!runtime->groupKey.valid() || runtime->groupKey != input.groupKey ||
            runtime->activationOutputLead != output.activationOutputLead ||
            !groups.insert(input.groupKey.value()).second ||
            !source.video.available || source.videoStreamIndex < 0 ||
            !source.video.decoder.preparedInputRetention ||
            runtime->audioPipeline.enabled != hasAudio || runtime->assembly.audio.has_value() != hasAudio ||
            runtime->audioPipeline.resolvedOutput.has_value() != hasAudio ||
            runtime->audioCorrection.has_value() != hasAudio || input.audioIdentity.has_value() != hasAudio ||
            (hasAudio && (!sameAudioFrames(*runtime->audioPipeline.resolvedOutput, aggregate.audio) ||
                runtime->assembly.audio->sourceIdentity != *input.audioIdentity)) ||
            runtime->assembly.video.sourceIdentity != input.videoIdentity ||
            !runtime->synchronization.startup.requireVideoKeyFrame || runtime->queues.frame == 0 ||
            !MediaAtomicOutputPolicyContract::accepts(runtime->edgePolicies.atomicMetadata) ||
            !MediaAtomicOutputPolicyContract::accepts(runtime->edgePolicies.preparedVideoFrame))
            return invalid("Composition source requires its own synchronized A/V source plan");
        if (hasAudio) {
            if (!MediaAtomicOutputPolicyContract::accepts(runtime->edgePolicies.audioDriftTransaction) ||
                runtime->edgePolicies.audioDriftTransaction.queuePolicy.capacity != runtime->queues.frame)
                return invalid("Composition source correction edges differ from planned queue bounds");
            auto sourceBounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(
                runtime->queues, runtime->audioPipeline);
            if (!sourceBounds) return ::media::Status::failure(sourceBounds.error());
            auto sourceCorrection = MediaAudioCorrectionReachabilityPlanner::plan(
                runtime->synchronization, MediaAudioSourceCorrectionFacts{
                    runtime->audioPipeline.resolvedOutput->sampleRate(), sourceBounds.value()});
            if (!sourceCorrection) return ::media::Status::failure(sourceCorrection.error());
            const auto& correction = sourceCorrection.value();
            if (!runtime->audioCorrection || *runtime->audioCorrection != correction.correction ||
                runtime->synchronization.audioServo.commandLeadNs != correction.commandLead ||
                runtime->synchronization.audioServo.compensationWindowNs != correction.compensationWindow ||
                runtime->synchronization.audioServo.frequencyFilterTimeConstantNs != correction.frequencyFilterTimeConstant)
                return invalid("Composition source correction differs from its actual source processing path");
        }
        const auto& frame = source.video.filterActive
            ? source.video.filter.outputFrame : source.video.decoder.outputFrame;
        const auto& tile = aggregate.canvas.tiles[i];
        if (!frame || frame->size.width != tile.width || frame->size.height != tile.height)
            return invalid("Composition source frame size differs from its planned tile");
        if (!runtime->synchronization.sourceClockMode || !runtime->synchronization.startup.videoReadiness)
            return invalid("Composition source transition requires its source clock authority");
        auto expectedTransition = MediaAvGenerationTransitionPlanner::planSourceContribution(
            *runtime->synchronization.sourceClockMode, runtime->audioPipeline.branchMode,
            source.video.filterActive, *runtime->synchronization.startup.videoReadiness, runtime->transition.acknowledgementTimeout, i == aggregate.audioSource);
        if (!expectedTransition) return ::media::Status::failure(expectedTransition.error());
        if (runtime->transition.participants != expectedTransition.value().participants)
            return invalid("Composition source transition differs from its exact source processing contract");
    }
    return ::media::Status::success();
}

MediaVideoOutputEncoderOptions videoOptions(
    const std::string& prefix, const MediaVideoOutputPlan& plan,
    const MediaRealtimeAvOutputRuntimePlan& runtime)
{
    return {prefix, plan, runtime.edgePolicies, runtime.queues.frame,
        *runtime.synchronization.startup.requireVideoKeyFrame};
}

::media::Result<MediaAudioEncodeBranchOptions> audioOptions(
    const std::string& prefix, const MediaRealtimeAvSourceRuntimePlan& runtime)
{
    MediaAudioBranchSegmentOptions branch;
    branch.prefix = prefix;
    branch.plan = runtime.audioPipeline;
    branch.queues = runtime.queues;
    branch.edgePolicies = runtime.edgePolicies;
    // InputGraphBuilder already emits normalized synchronized releases.
    branch.normalizeInputPackets = false;
    auto status = mapSynchronizedAudioSourceOptions(runtime, branch);
    if (!status) return ::media::Result<MediaAudioEncodeBranchOptions>::failure(status.error());
    return ::media::Result<MediaAudioEncodeBranchOptions>::success(makeAudioEncodeBranchOptions(branch));
}

::media::Status addAggregatePorts(MediaGraph& graph, MediaNodeId node,
                                  const MediaAvContinuousAggregateTopology& plan)
{
    using namespace MediaGraphBuildSupport;
    const auto input = [&](const std::string& port, MediaStreamKind stream,
                           MediaEdgeKind edge, MediaPayloadKind payload) {
        return addInputPortChecked(graph, owner, node, port, stream, edge, payload, true, false);
    };
    for (std::size_t i = 0; i < plan.sources.size(); ++i) {
        if (auto status = input(plan.sources[i].videoPort, MediaStreamKind::Video,
                MediaEdgeKind::RawFrame, MediaPayloadKind::Frame); !status) return status;
        if (auto status = input("source_epoch_" + std::to_string(i), MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return status;
        if (plan.sources[i].discardedAudioPort) {
            if (auto status = input(*plan.sources[i].discardedAudioPort, MediaStreamKind::Audio,
                    MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame); !status) return status;
        }
    }
    for (const auto& [port, stream] : {std::pair{"video_codec", MediaStreamKind::Video}}) {
        if (auto status = input(port, stream, MediaEdgeKind::Metadata,
                MediaPayloadKind::CodecContext); !status) return status;
    }
    for (const auto& [port, stream, edge, payload, multiple] : {
             std::tuple{"video", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false},
             std::tuple{"activated", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true}}) {
        if (auto status = addOutputPortChecked(graph, owner, node, port, stream,
                edge, payload, true, multiple); !status) return status;
    }
    return ::media::Status::success();
}

template<class Bind>
::media::Status connectAggregate(MediaGraph& graph,
    const std::vector<MediaCompositionAggregateConnection>& connections, const Bind& bind)
{
    for (const auto& connection : connections) {
        auto from = bind(connection.from);
        auto to = bind(connection.to);
        if (!connection.output.empty()) from.port = connection.output;
        if (!connection.input.empty()) to.port = connection.input;
        const auto* output = graph.findOutputPort(from.node, from.port);
        const auto* input = graph.findInputPort(to.node, to.port);
        if (!output || !input || output->streamKind != connection.stream ||
            input->streamKind != connection.stream || output->payloadKind != connection.payload ||
            input->payloadKind != connection.payload)
            return invalid("Aggregate connection role differs from its actual endpoint contract");
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, from.node, from.port,
                to.node, to.port, from.port + " -> " + to.port, connection.policy); !status) return status;
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaRealtimeCompositionTopology> MediaRealtimeCompositionGraphBuilder::buildTopology(
    const std::string& prefix, MediaRealtimeCompositionGraphOptions options)
{
    using Result = ::media::Result<MediaRealtimeCompositionTopology>;
    MediaGraph graph;
    if (prefix.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Composition assembly requires an empty graph and explicit node prefix"));
    if (auto status = validateOptions(options); !status) return Result::failure(status.error());
    const auto& aggregate = options.aggregate;
    auto& outputRuntime = options.outputRuntime;
    std::vector<MediaRealtimeEdgePolicySet> sourcePolicies;
    for (const auto& source : options.sources) sourcePolicies.push_back(source.runtime.edgePolicies);
    auto aggregateConnections = MediaCompositionAggregateConnectionsPlanner::plan(
        aggregate.sources, aggregate.audioSource, aggregate.audioPort, sourcePolicies, outputRuntime.edgePolicies);
    if (!aggregateConnections) return Result::failure(aggregateConnections.error());
    std::vector<MediaCompositionSourceTopology> sourceTopologies;
    for (std::size_t i=0; i<options.sources.size(); ++i) {
        const auto& source = options.sources[i];
        const auto& sync = source.runtime.synchronization;
        if (!sync.members || !sync.startup.videoReadiness || !sync.sourceClockMode)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Source topology requires declared RTP startup facts"));
        const MediaCompositionSourceTopologyFacts facts{
            *sync.members, *sync.startup.videoReadiness, *sync.sourceClockMode,
            source.runtime.edgePolicies, source.branchConnections, source.inputConnections,
            aggregateConnections.value().sources[i]};
        auto projected=MediaCompositionSourceTopologyPlanner::plan(facts);
        if (!projected) return Result::failure(projected.error());
        sourceTopologies.push_back(std::move(projected).value());
    }
    const auto videoShape=MediaVideoBranchConnectionsPlanner::outputEncoder();
    const auto audioShape=MediaAudioBranchConnectionsPlanner::outputEncoder(true);
    const auto& policies=outputRuntime.edgePolicies;
    auto videoConnections=MediaVideoBranchConnectionsPlanner::plan(videoShape,{policies.metadata,
        policies.videoPacket,policies.videoFrame,policies.videoFrame,policies.preparedVideoFrame});
    if(!videoConnections)return Result::failure(videoConnections.error());
    auto audioConnections=MediaAudioBranchConnectionsPlanner::plan(audioShape,policies);
    if(!audioConnections)return Result::failure(audioConnections.error());
    auto schedulerDeclaration=MediaOutputSegmentPlanner::scheduler(policies,outputRuntime.canonicalAudioInputPolicy);
    if(!schedulerDeclaration)return Result::failure(schedulerDeclaration.error());
    auto executionDeclaration=MediaOutputSegmentPlanner::execution(policies);
    if(!executionDeclaration)return Result::failure(executionDeclaration.error());
    auto protocolDeclaration=[&]()->::media::Result<MediaOutputSegmentDeclaration>{
        if(outputRuntime.outputAdapter==MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp)
            return MediaOutputSegmentPlanner::rtp(true,MediaPayloadKind::CodecContext,policies);
        const auto* mpegts=std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(&outputRuntime.protocolOutput);
        if(outputRuntime.outputAdapter!=MediaAvSyncOutputAdapterKind::ProjectMpegTs || !mpegts)
            return ::media::Result<MediaOutputSegmentDeclaration>::failure(::media::ErrorInfo::invalidArgument(
                "Composition output has no declared protocol topology"));
        return MediaOutputSegmentPlanner::mpegTs(true,std::holds_alternative<MediaMpegTsRtpOutputPlan>(mpegts->transport),
            MediaPayloadKind::CodecContext,policies);
    }();
    if(!protocolDeclaration)return Result::failure(protocolDeclaration.error());
    auto graphTopology=MediaCompositionGraphTopologyPlanner::plan({sourceTopologies,aggregateConnections.value(),
        videoShape,videoConnections.value(),audioShape,audioConnections.value(),schedulerDeclaration.value(),
        protocolDeclaration.value(),executionDeclaration.value()});
    if(!graphTopology)return Result::failure(graphTopology.error());
    MediaCompositionGraphBindings graphBindings(graphTopology.value());
    using GraphDomain=MediaCompositionGraphDomain;
    using OutputRole=MediaOutputSegmentRole;
    const auto attached=[&](GraphDomain domain,auto role){return graphBindings.attached(compositionOutputRole(domain,role));};
    auto outputVideo = MediaVideoTranscodeBranchBuilder::buildOutputEncoder(graph,
        videoOptions(prefix + ".output.video", options.outputVideo, outputRuntime));
    if (!outputVideo) return Result::failure(outputVideo.error());
    auto outputAudio = MediaAudioEncodeBranchBuilder::buildOutputEncoder(graph,
        {prefix + ".output.audio", outputRuntime.audioOutput,
         outputRuntime.edgePolicies, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
         outputRuntime.queues.frame, outputRuntime.encoderFifoRetention, outputRuntime.groupKey});
    if (!outputAudio) return Result::failure(outputAudio.error());
    const auto aggregateNode = graph.addNode(*MediaCompositionAggregateConnectionsPlanner::nodeKind(MediaCompositionAggregateRole::VideoAggregate),
        prefix + ".aggregate", "Continuous A/V aggregate");
    auto aggregateCredits = MediaAvAggregateFrameCreditPlanner::encode(aggregate);
    if (!aggregateCredits) return Result::failure(aggregateCredits.error());
    graph.findNode(aggregateNode)->options = std::move(aggregateCredits).value();
    if (auto status = addAggregatePorts(graph, aggregateNode, aggregate); !status)
        return Result::failure(status.error());
    const auto audioAggregate = graph.addNode(*MediaCompositionAggregateConnectionsPlanner::nodeKind(MediaCompositionAggregateRole::AudioAggregate),
        prefix + ".aggregate.audio", "Continuous audio aggregate");
    graph.findNode(audioAggregate)->options = graph.findNode(aggregateNode)->options;
    using namespace MediaGraphBuildSupport;
    for (const auto& [port, stream, edge, payload] : {
        std::tuple{aggregate.audioPort, MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame},
        std::tuple{std::string("audio_codec"), MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
        std::tuple{std::string("activated"), MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent}}) {
        if (auto status = addInputPortChecked(graph, owner, audioAggregate, port, stream, edge, payload, true, false); !status)
            return Result::failure(status.error());
    }
    if (auto status = addOutputPortChecked(graph, owner, audioAggregate, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, false); !status) return Result::failure(status.error());
    using AggregateRole = MediaCompositionAggregateRole;
    if(!outputVideo.value().encoded.bindings || !outputAudio.value().encoded.bindings)
        return Result::failure(::media::ErrorInfo::invalidArgument("Composition output lacks actual branch bindings"));
    const auto* videoBindings=std::get_if<MediaVideoBranchBindings>(&*outputVideo.value().encoded.bindings);
    const auto* audioBindings=std::get_if<MediaAudioBranchBindings>(&*outputAudio.value().encoded.bindings);
    if(!videoBindings || !audioBindings)return Result::failure(::media::ErrorInfo::invalidArgument("Composition output binding type differs"));
    graphBindings.bindOutput(GraphDomain::VideoOutput,[&](std::size_t role){return videoBindings->endpoint(static_cast<MediaVideoBranchRole>(role));});
    graphBindings.bindOutput(GraphDomain::AudioOutput,[&](std::size_t role){return audioBindings->endpoint(static_cast<MediaAudioBranchRole>(role));});
    // Bind owners before resolving the declared external attachments.
    graphBindings.bindOutput(GraphDomain::Aggregate,[&](std::size_t role)->MediaEndpoint{
        if(static_cast<AggregateRole>(role)==AggregateRole::VideoAggregate)return {aggregateNode,""};
        if(static_cast<AggregateRole>(role)==AggregateRole::AudioAggregate)return {audioAggregate,""};
        return attached(GraphDomain::Aggregate,static_cast<AggregateRole>(role));
    });
    const auto outputEndpoint=[&](AggregateRole role){return attached(GraphDomain::Aggregate,role);};
    std::vector<MediaNodeId> outputMembers{aggregateNode, audioAggregate};
    for (const auto* branch : {&outputVideo.value(), &outputAudio.value()}) {
        if (!branch->encoded.processing.source.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition encoder segment unexpectedly owns source nodes"));
        const auto& members = branch->encoded.processing.output;
        outputMembers.insert(outputMembers.end(), members.begin(), members.end());
    }
    std::vector<MediaAvSourceDomainRegistration> registrations;
    std::vector<MediaRealtimeCompositionSourceTargets> targets;
    std::vector<MediaRealtimeInputGraph> sourceInputs;
    for (std::size_t i = 0; i < options.sources.size(); ++i) {
        const auto& plan = options.sources[i];
        const auto& runtime = plan.runtime;
        const auto sourcePrefix = prefix + ".source_" + std::to_string(i);
        auto inputs = MediaRealtimeInputGraphBuilder::append(graph, sourcePrefix, plan);
        if (!inputs) return Result::failure(inputs.error());
        if (!inputs.value().synchronized) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition source input lacks synchronized release endpoints"));
        MediaVideoSourceBranchOptions video;
        video.prefix = sourcePrefix + ".video";
        video.plan = plan.video;
        video.sourceStreamIndex = plan.videoStreamIndex;
        const bool decoded = std::holds_alternative<MediaDecodedVideoAssemblyTiming>(runtime.assembly.video.timing);
        video.timing = decoded ? MediaVideoSourceTiming{MediaSourcePresentationTiming{}}
                               : MediaVideoSourceTiming{MediaObservedVideoCadence{plan.frameRate}};
        video.maximumFrameDuplicationGap = plan.maximumFrameDuplicationGap;
        video.queues = runtime.queues;
        video.edgePolicies = runtime.edgePolicies;
        video.canonicalLineageCapacity = runtime.queues.frame;
        video.generationStartRequiresKeyFrame = runtime.synchronization.startup.requireVideoKeyFrame;
        video.formatSourceNode = inputs.value().videoFormat.node;
        video.formatSourcePort = inputs.value().videoFormat.port;
        video.packetSourceNode = inputs.value().videoPacket.node;
        video.packetSourcePort = inputs.value().videoPacket.port;
        auto sourceVideo = [&]() -> ::media::Result<MediaVideoSourceBranchEndpoints> {
            if (!decoded) return MediaVideoTranscodeBranchBuilder::buildSource(graph, video, outputEndpoint(AggregateRole::OutputVideoCodec));
            if (inputs.value().videoPacket.valid() || !inputs.value().videoFrame.valid())
                return ::media::Result<MediaVideoSourceBranchEndpoints>::failure(::media::ErrorInfo::invalidArgument(
                    "Decoded composition source requires an exclusive released frame endpoint"));
            auto frames = MediaVideoTranscodeBranchBuilder::buildSourceFrames(
                graph, video, inputs.value().videoFrame, outputEndpoint(AggregateRole::OutputVideoCodec), runtime.edgePolicies.preparedVideoFrame);
            if (!frames) return ::media::Result<MediaVideoSourceBranchEndpoints>::failure(frames.error());
            return ::media::Result<MediaVideoSourceBranchEndpoints>::success({frames.value().frame,
                MediaNodeId::invalid(), frames.value().preparationOwner, std::move(frames.value().processing),
                {inputs.value().synchronized->decodedVideoBindings, std::move(frames.value().bindings)}});
        }();
        if (!sourceVideo) return Result::failure(sourceVideo.error());
        std::optional<MediaAudioSourceBranchEndpoints> sourceAudio;
        if (runtime.audioPipeline.enabled) {
            auto audio = audioOptions(sourcePrefix + ".audio", runtime);
            if (!audio) return Result::failure(audio.error());
            audio.value().formatSourceNode = inputs.value().audioFormat.node;
            audio.value().formatSourcePort = inputs.value().audioFormat.port;
            audio.value().packetSourceNode = inputs.value().audioPacket.node;
            audio.value().packetSourcePort = inputs.value().audioPacket.port;
            auto built = MediaAudioEncodeBranchBuilder::buildSource(graph, audio.value(), outputEndpoint(AggregateRole::OutputAudioCodec));
            if (!built) return Result::failure(built.error());
            sourceAudio = std::move(built).value();
        }
        if (!sourceVideo.value().bindings.decoder)
            return Result::failure(::media::ErrorInfo::invalidArgument("Composition source lacks its built decoder role bindings"));
        if (!sourceVideo.value().startupPreparationOwner || !sourceVideo.value().processing.output.empty() ||
            (sourceAudio && !sourceAudio->processing.output.empty())) return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition source must own preparation and must not own encoders"));
        const auto sourceEndpoint = [&](AggregateRole role) -> MediaEndpoint {
            switch (role) {
            case AggregateRole::SourceVideoFrame: return sourceVideo.value().frame;
            case AggregateRole::SourceAudioFrame:
                return sourceAudio ? sourceAudio->frame : MediaEndpoint{MediaNodeId::invalid(), ""};
            case AggregateRole::SourceActivated: return inputs.value().synchronized->activatedRelease;
            default: return outputEndpoint(role);
            }
        };
        if (auto status = connectAggregate(graph, aggregateConnections.value().sources[i], sourceEndpoint); !status)
            return Result::failure(status.error());
        auto members = inputs.value().sourceMembers;
        members.insert(members.end(), sourceVideo.value().processing.source.begin(), sourceVideo.value().processing.source.end());
        if (sourceAudio) members.insert(members.end(), sourceAudio->processing.source.begin(), sourceAudio->processing.source.end());
        std::vector<MediaEndpoint> topologyEndpoints;
        const auto& inputBinding=inputs.value().synchronized->bindings;
        const auto decoderFrame=MediaEndpoint{sourceVideo.value().bindings.decoder->endpoint(MediaVideoBranchRole::Decode).node,"frame"};
        for (const auto& binding : sourceTopologies[i].bindings) {
            using D=MediaSourceTopologyDomain;
            switch (binding.role.domain) {
            case D::Input:
                topologyEndpoints.push_back(inputBinding.endpoint(static_cast<MediaSourceInputRole>(binding.role.role),decoderFrame)); break;
            case D::Startup:
                topologyEndpoints.push_back({inputBinding.shared.routeNode(static_cast<MediaStartupRouteRole>(binding.role.role)).value_or(MediaNodeId::invalid()),""}); break;
            case D::Rtp:
                topologyEndpoints.push_back({inputBinding.protocol.rtp ? inputBinding.protocol.rtp->routeNode(static_cast<MediaRtpClockRouteRole>(binding.role.role)) : MediaNodeId::invalid(),""}); break;
            case D::VideoDecoder:
                topologyEndpoints.push_back(sourceVideo.value().bindings.decoder->endpoint(static_cast<MediaVideoBranchRole>(binding.role.role))); break;
            case D::VideoFrames:
                topologyEndpoints.push_back(sourceVideo.value().bindings.frames.endpoint(static_cast<MediaVideoBranchRole>(binding.role.role))); break;
            case D::Audio:
                topologyEndpoints.push_back(sourceAudio ? sourceAudio->bindings.endpoint(static_cast<MediaAudioBranchRole>(binding.role.role)) : MediaEndpoint{}); break;
            case D::Aggregate:
                topologyEndpoints.push_back(sourceEndpoint(static_cast<AggregateRole>(binding.role.role))); break;
            }
        }
        if(auto status=graphBindings.bindSource(i,sourceTopologies[i],topologyEndpoints);!status)
            return Result::failure(status.error());
        const auto isolatedAudio = sourceAudio && inputs.value().audioFormat.node != inputs.value().videoFormat.node
            ? std::optional<MediaNodeId>(inputs.value().audioFormat.node) : std::nullopt;
        targets.push_back({i, inputs.value().videoFormat.node, isolatedAudio, members});
        registrations.push_back({inputs.value().synchronized->registration,
            *sourceVideo.value().startupPreparationOwner, std::move(members)});
        sourceInputs.push_back(std::move(inputs).value());
    }
    if (auto status = connectAggregate(graph, aggregateConnections.value().output, outputEndpoint); !status)
        return Result::failure(status.error());
    const MediaRealtimeAvOutputSegmentPlan outputSegment{
        outputRuntime.groupKey, outputRuntime.edgePolicies, outputRuntime.audioOutput.branchMode(), outputRuntime.canonicalAudioInputPolicy,
        outputRuntime.outputAdapter, outputRuntime.scheduling, outputRuntime.protocolOutput, outputRuntime.datagramTransport};
    auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(graph,
        {prefix + ".output.scheduler", attached(GraphDomain::Scheduler,OutputRole::VideoPacket), attached(GraphDomain::Scheduler,OutputRole::AudioPacket)},
        outputSegment);
    if (!scheduled) return Result::failure(scheduled.error());
    outputMembers.insert(outputMembers.end(), scheduled.value().outputMembers.begin(), scheduled.value().outputMembers.end());
    graphBindings.bindOutput(GraphDomain::Scheduler,[&](std::size_t role){return scheduled.value().bindings.endpoint(static_cast<OutputRole>(role));});
    std::optional<MediaNodeId> publisher;
    const auto bindProtocol=[&](const auto& protocol){
        graphBindings.bindOutput(GraphDomain::Execution,[&](std::size_t role){return protocol.executionBindings.endpoint(static_cast<OutputRole>(role));});
        graphBindings.bindOutput(GraphDomain::Protocol,[&](std::size_t role){return protocol.bindings.endpoint(static_cast<OutputRole>(role));});
    };
    if (outputRuntime.outputAdapter == MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        auto protocol = MediaScheduledRtpOutputSegmentBuilder::build(graph,
            {prefix + ".output.rtp", attached(GraphDomain::Protocol,OutputRole::Activation),
             attached(GraphDomain::Protocol,OutputRole::VideoCodec), attached(GraphDomain::Protocol,OutputRole::AudioCodec),
             attached(GraphDomain::Protocol,OutputRole::VideoPacket), attached(GraphDomain::Protocol,OutputRole::AudioPacket)}, outputSegment);
        if (!protocol) return Result::failure(protocol.error());
        bindProtocol(protocol.value());
        outputMembers.insert(outputMembers.end(), protocol.value().outputMembers.begin(), protocol.value().outputMembers.end());
    } else if (outputRuntime.outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        auto protocol = MediaScheduledMpegTsOutputSegmentBuilder::build(graph,
            {prefix + ".output.mpegts", attached(GraphDomain::Protocol,OutputRole::Activation),
             attached(GraphDomain::Protocol,OutputRole::VideoCodec), attached(GraphDomain::Protocol,OutputRole::AudioCodec),
             attached(GraphDomain::Protocol,OutputRole::SerializedPacket), true, outputRuntime.audioOutput.branchMode() == MediaBranchMode::TranscodeFrame}, outputSegment);
        if (!protocol) return Result::failure(protocol.error());
        bindProtocol(protocol.value());
        outputMembers.insert(outputMembers.end(), protocol.value().outputMembers.begin(), protocol.value().outputMembers.end());
        if (protocol.value().rtpSdpPublisher.isValid()) publisher = protocol.value().rtpSdpPublisher;
    } else {
        return Result::failure(::media::ErrorInfo::unsupported("Composition output requires a scheduled protocol adapter"));
    }
    // One complete graph comparison owns structural checks. Input validation
    // remains responsible only for runtime registration identity.
    std::vector<std::vector<MediaNodeId>> sourceMembers;
    for(std::size_t i=0;i<sourceInputs.size();++i){
        if(auto status=MediaCompositionSourceInputValidator::validate(sourceInputs[i]);!status)
            return Result::failure(status.error());
        sourceMembers.push_back(targets[i].sourceMembers);
    }
    auto identity=MediaCompositionGraphTopologyValidator::validate(graph,graphTopology.value(),
        graphBindings.endpoints(),sourceMembers,outputMembers);
    if(!identity)return Result::failure(identity.error());
    MediaAvOutputDomainRegistration outputRegistration{
        aggregateNode, audioAggregate, scheduled.value().scheduler, publisher, std::move(outputMembers)};
    MediaRealtimeCompositionTopology topology(std::move(graph), std::move(options),
        std::move(registrations), std::move(outputRegistration), std::move(targets),
        std::move(graphTopology).value(),std::move(identity).value());
    if (auto sources = topology.planSourceResources(); !sources) return Result::failure(sources.error());
    if (auto retention = topology.planCanvasRetention(); !retention) return Result::failure(retention.error());
    return Result::success(std::move(topology));
}

} // namespace media::ffmpeg::graph
