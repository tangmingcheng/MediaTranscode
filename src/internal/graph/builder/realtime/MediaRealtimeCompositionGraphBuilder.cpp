#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeInputGraphBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"

#include <algorithm>
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
    if (!options.aggregate || !options.preparedVideoEncoder || options.sources.empty())
        return invalid("Composition requires planned sources, aggregate and prepared output encoder");
    const auto& aggregate = *options.aggregate;
    const auto& output = options.outputRuntime;
    const auto& encoder = options.outputVideo.selected.encoder.encoderOpenContract;
    if (aggregate.sources.size() != options.sources.size() ||
        aggregate.canvas.tiles.size() != options.sources.size() ||
        aggregate.audioSource >= options.sources.size() ||
        output.groupKey != aggregate.outputGroupKey || !output.groupKey.valid() ||
        !options.outputVideo.enabled || options.outputVideo.branchMode != MediaBranchMode::TranscodeFrame ||
        !encoder || encoder->width != aggregate.canvas.width || encoder->height != aggregate.canvas.height ||
        encoder->frameRate != aggregate.videoFrameRate || !output.audioPipeline.enabled ||
        !output.audioPipeline.resolvedOutput ||
        !sameAudioFrames(*output.audioPipeline.resolvedOutput, aggregate.audio))
        return invalid("Composition output contracts disagree with the aggregate canvas or audio grid");
    if (!MediaAtomicOutputPolicyContract::accepts(output.edgePolicies.atomicMetadata))
        return invalid("Composition activation requires the planned atomic metadata policy");
    std::unordered_set<std::string> groups{output.groupKey.value()};
    std::unordered_set<std::string> ports{"video_codec", "audio_codec", aggregate.audioPort};
    if (aggregate.audioPort.empty()) return invalid("Composition requires a selected audio port");
    for (std::size_t i = 0; i < options.sources.size(); ++i) {
        const auto& source = options.sources[i];
        const auto* runtime = std::get_if<MediaRealtimeAvSyncRuntimePlan>(&source.runtime);
        const auto& input = aggregate.sources[i];
        if (!runtime || !runtime->groupKey.valid() || runtime->groupKey != input.groupKey ||
            !groups.insert(input.groupKey.value()).second ||
            !source.videoPlan.enabled || source.videoPlan.branchMode != MediaBranchMode::TranscodeFrame ||
            !runtime->audioPipeline.enabled || !runtime->audioPipeline.resolvedOutput ||
            !sameAudioFrames(*runtime->audioPipeline.resolvedOutput, aggregate.audio) ||
            !runtime->synchronization.startup.requireVideoKeyFrame || runtime->queues.frame == 0 ||
            !MediaAtomicOutputPolicyContract::accepts(runtime->edgePolicies.atomicMetadata) ||
            !MediaAtomicOutputPolicyContract::accepts(runtime->edgePolicies.preparedVideoFrame))
            return invalid("Composition source requires its own complete synchronized A/V transcode plan");
        const auto& frame = source.videoPlan.filterActive
            ? source.videoPlan.selected.filter.outputFrame : source.videoPlan.selected.decoder.outputFrame;
        const auto& tile = aggregate.canvas.tiles[i];
        if (!frame || frame->size.width != tile.width || frame->size.height != tile.height)
            return invalid("Composition source frame size differs from its planned tile");
        if (input.videoPort.empty() || !ports.insert(input.videoPort).second ||
            !ports.insert("source_epoch_" + std::to_string(i)).second ||
            (i == aggregate.audioSource) == input.discardedAudioPort.has_value() ||
            (input.discardedAudioPort && (input.discardedAudioPort->empty() ||
             !ports.insert(*input.discardedAudioPort).second)))
            return invalid("Composition requires distinct frame and epoch ports including all nonselected audio");
        bool aggregateParticipant = false;
        for (const auto& participant : runtime->transition.participants) {
            if (participant.participant == MediaAvGenerationParticipant::CanonicalLineage) {
                aggregateParticipant = std::find(participant.requiredChildren.begin(),
                    participant.requiredChildren.end(), "aggregate_source") != participant.requiredChildren.end();
            } else if (participant.participant != MediaAvGenerationParticipant::AudioCorrection) {
                return invalid("Composition source transition must not reset output participants");
            }
        }
        if (!aggregateParticipant)
            return invalid("Composition source transition lacks its aggregate purge child");
    }
    return ::media::Status::success();
}

MediaVideoTranscodeBranchOptions videoOptions(
    const std::string& prefix, const MediaPipelinePlan& plan,
    const MediaVideoTranscodeParameters& parameters, const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    MediaVideoTranscodeBranchOptions options;
    options.prefix = prefix;
    options.plan = plan;
    options.parameters = parameters;
    options.queues = runtime.queues;
    options.edgePolicies = runtime.edgePolicies;
    options.canonicalLineageCapacity = runtime.queues.frame;
    options.generationStartRequiresKeyFrame = runtime.synchronization.startup.requireVideoKeyFrame;
    return options;
}

::media::Result<MediaAudioEncodeBranchOptions> audioOptions(
    const std::string& prefix, const MediaRealtimeAvSyncRuntimePlan& runtime)
{
    MediaAudioBranchSegmentOptions branch;
    branch.prefix = prefix;
    branch.plan = runtime.audioPipeline;
    branch.queues = runtime.queues;
    branch.edgePolicies = runtime.edgePolicies;
    // InputGraphBuilder already emits normalized synchronized releases.
    branch.normalizeInputPackets = false;
    auto status = mapSynchronizedAudioBranchOptions(runtime, branch);
    if (!status) return ::media::Result<MediaAudioEncodeBranchOptions>::failure(status.error());
    return ::media::Result<MediaAudioEncodeBranchOptions>::success(makeAudioEncodeBranchOptions(branch));
}

::media::Status addAggregatePorts(MediaGraph& graph, MediaNodeId node,
                                  const MediaAvContinuousAggregatePlan& plan)
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
    if (auto status = input(plan.audioPort, MediaStreamKind::Audio,
            MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame); !status) return status;
    for (const auto& [port, stream] : {std::pair{"video_codec", MediaStreamKind::Video},
                                      std::pair{"audio_codec", MediaStreamKind::Audio}}) {
        if (auto status = input(port, stream, MediaEdgeKind::Metadata,
                MediaPayloadKind::CodecContext); !status) return status;
    }
    for (const auto& [port, stream, edge, payload, multiple] : {
             std::tuple{"video", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false},
             std::tuple{"audio", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, false},
             std::tuple{"activated", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true}}) {
        if (auto status = addOutputPortChecked(graph, owner, node, port, stream,
                edge, payload, true, multiple); !status) return status;
    }
    return ::media::Status::success();
}

::media::Status connect(MediaGraph& graph, MediaEndpoint from, MediaEndpoint to,
                        const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(graph, owner, from.node, from.port,
        to.node, to.port, from.port + " -> " + to.port, policy);
}

} // namespace

::media::Result<MediaRealtimeCompositionGraphAssembly> MediaRealtimeCompositionGraphBuilder::append(
    MediaGraph& graph, const std::string& prefix, MediaRealtimeCompositionGraphOptions options)
{
    using Result = ::media::Result<MediaRealtimeCompositionGraphAssembly>;
    if (!graph.empty() || prefix.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Composition assembly requires an empty graph and explicit node prefix"));
    if (auto status = validateOptions(options); !status) return Result::failure(status.error());
    const auto& aggregate = *options.aggregate;
    auto& outputRuntime = options.outputRuntime;
    auto outputVideo = MediaVideoTranscodeBranchBuilder::buildOutputEncoder(graph,
        videoOptions(prefix + ".output.video", options.outputVideo, options.outputVideoParameters, outputRuntime));
    if (!outputVideo) return Result::failure(outputVideo.error());
    auto outputAudioOptions = audioOptions(prefix + ".output.audio", outputRuntime);
    if (!outputAudioOptions) return Result::failure(outputAudioOptions.error());
    auto outputAudio = MediaAudioEncodeBranchBuilder::buildOutputEncoder(graph, outputAudioOptions.value());
    if (!outputAudio) return Result::failure(outputAudio.error());
    const auto aggregateNode = graph.addNode(MediaNodeKind::AvContinuousAggregate,
        prefix + ".aggregate", "Continuous A/V aggregate");
    if (auto status = addAggregatePorts(graph, aggregateNode, aggregate); !status)
        return Result::failure(status.error());
    std::vector<MediaNodeId> outputMembers{aggregateNode};
    for (const auto* branch : {&outputVideo.value(), &outputAudio.value()}) {
        if (!branch->encoded.processing.source.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition encoder segment unexpectedly owns source nodes"));
        const auto& members = branch->encoded.processing.output;
        outputMembers.insert(outputMembers.end(), members.begin(), members.end());
    }
    std::vector<MediaAvRuntimeDomainBinding> domains;
    std::vector<MediaRealtimeCompositionSourceTargets> targets;
    for (std::size_t i = 0; i < options.sources.size(); ++i) {
        const auto& plan = options.sources[i];
        const auto& runtime = std::get<MediaRealtimeAvSyncRuntimePlan>(plan.runtime);
        const auto sourcePrefix = prefix + ".source_" + std::to_string(i);
        auto inputs = MediaRealtimeInputGraphBuilder::append(graph, sourcePrefix, plan);
        if (!inputs) return Result::failure(inputs.error());
        if (!inputs.value().synchronized) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition source input lacks synchronized release endpoints"));
        auto video = videoOptions(sourcePrefix + ".video", plan.videoPlan, plan.videoParameters, runtime);
        video.formatSourceNode = inputs.value().videoFormat.node;
        video.formatSourcePort = inputs.value().videoFormat.port;
        video.packetSourceNode = inputs.value().videoPacket.node;
        video.packetSourcePort = inputs.value().videoPacket.port;
        auto sourceVideo = MediaVideoTranscodeBranchBuilder::buildSource(graph, video, outputVideo.value().codec);
        if (!sourceVideo) return Result::failure(sourceVideo.error());
        auto audio = audioOptions(sourcePrefix + ".audio", runtime);
        if (!audio) return Result::failure(audio.error());
        audio.value().formatSourceNode = inputs.value().audioFormat.node;
        audio.value().formatSourcePort = inputs.value().audioFormat.port;
        audio.value().packetSourceNode = inputs.value().audioPacket.node;
        audio.value().packetSourcePort = inputs.value().audioPacket.port;
        auto sourceAudio = MediaAudioEncodeBranchBuilder::buildSource(graph, audio.value(), outputAudio.value().codec);
        if (!sourceAudio) return Result::failure(sourceAudio.error());
        if (!sourceVideo.value().startupPreparationOwner || !sourceVideo.value().processing.output.empty() ||
            !sourceAudio.value().processing.output.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition source must own preparation and must not own encoders"));
        const auto& sourcePlan = aggregate.sources[i];
        if (auto status = connect(graph, sourceVideo.value().frame, {aggregateNode, sourcePlan.videoPort},
                runtime.edgePolicies.preparedVideoFrame); !status) return Result::failure(status.error());
        if (auto status = connect(graph, inputs.value().synchronized->activatedRelease,
                {aggregateNode, "source_epoch_" + std::to_string(i)},
                runtime.edgePolicies.atomicMetadata); !status) return Result::failure(status.error());
        const auto& audioPort = i == aggregate.audioSource ? aggregate.audioPort : *sourcePlan.discardedAudioPort;
        if (auto status = connect(graph, sourceAudio.value().frame, {aggregateNode, audioPort},
                runtime.edgePolicies.audioFrame); !status) return Result::failure(status.error());
        auto members = inputs.value().sourceMembers;
        for (const auto* branch : {&sourceVideo.value(), &sourceAudio.value()})
            members.insert(members.end(), branch->processing.source.begin(), branch->processing.source.end());
        const auto isolatedAudio = inputs.value().audioFormat.node != inputs.value().videoFormat.node
            ? std::optional<MediaNodeId>(inputs.value().audioFormat.node) : std::nullopt;
        targets.push_back({i, inputs.value().videoFormat.node, isolatedAudio, members});
        domains.push_back({runtime.groupKey, runtime.synchronization,
            MediaAvSourceDomainBinding{runtime.transition,
                {inputs.value().synchronized->registration, *sourceVideo.value().startupPreparationOwner,
                 std::move(members)}, nullptr}});
    }
    const auto& policies = outputRuntime.edgePolicies;
    for (const auto& [from, to, policy] : {
             std::tuple{outputVideo.value().codec, MediaEndpoint{aggregateNode, "video_codec"}, policies.metadata},
             std::tuple{outputAudio.value().codec, MediaEndpoint{aggregateNode, "audio_codec"}, policies.metadata},
             std::tuple{MediaEndpoint{aggregateNode, "video"}, outputVideo.value().frameInput, policies.preparedVideoFrame},
             std::tuple{MediaEndpoint{aggregateNode, "audio"}, outputAudio.value().frameInput, policies.audioFrame}}) {
        if (auto status = connect(graph, from, to, policy); !status) return Result::failure(status.error());
    }
    auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(graph,
        {prefix + ".output.scheduler", outputVideo.value().encoded.packet, outputAudio.value().encoded.packet},
        outputRuntime);
    if (!scheduled) return Result::failure(scheduled.error());
    outputMembers.insert(outputMembers.end(), scheduled.value().outputMembers.begin(), scheduled.value().outputMembers.end());
    std::optional<MediaNodeId> publisher;
    const MediaEndpoint activated{aggregateNode, "activated"};
    if (outputRuntime.outputAdapter == MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        auto protocol = MediaScheduledRtpOutputSegmentBuilder::build(graph,
            {prefix + ".output.rtp", activated, outputVideo.value().encoded.codec, outputAudio.value().encoded.codec,
             scheduled.value().video, scheduled.value().audio}, outputRuntime);
        if (!protocol) return Result::failure(protocol.error());
        outputMembers.insert(outputMembers.end(), protocol.value().outputMembers.begin(), protocol.value().outputMembers.end());
    } else if (outputRuntime.outputAdapter == MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
        auto protocol = MediaScheduledMpegTsOutputSegmentBuilder::build(graph,
            {prefix + ".output.mpegts", activated, outputVideo.value().encoded.codec, outputAudio.value().encoded.codec,
             scheduled.value().serialized, options.outputVideo.enabled, outputRuntime.audioPipeline.enabled}, outputRuntime);
        if (!protocol) return Result::failure(protocol.error());
        outputMembers.insert(outputMembers.end(), protocol.value().outputMembers.begin(), protocol.value().outputMembers.end());
        if (protocol.value().rtpSdpPublisher.isValid()) publisher = protocol.value().rtpSdpPublisher;
    } else {
        return Result::failure(::media::ErrorInfo::unsupported("Composition output requires a scheduled protocol adapter"));
    }
    domains.push_back({outputRuntime.groupKey, outputRuntime.synchronization,
        MediaAvOutputDomainBinding{{aggregateNode, scheduled.value().scheduler, publisher, outputMembers},
            options.aggregate, options.preparedVideoEncoder}});
    auto outputProduct = std::visit([]<typename Product>(Product&& product) -> MediaAvSyncRuntimeOutputProduct {
        return MediaAvSyncRuntimeOutputProduct(std::forward<Product>(product));
    }, std::move(outputRuntime.protocolOutput));
    MediaAvSyncRuntimeBinding binding{std::move(domains), outputRuntime.groupKey, policies,
        outputRuntime.datagramTransport, MediaSynchronizedAudioExecutionProduct::FrameTranscode, std::move(outputProduct)};
    if (auto status = MediaAvSyncGraphShapeValidator::validate(graph, binding); !status)
        return Result::failure(status.error());
    return Result::success({std::move(binding), std::move(targets), std::move(outputMembers)});
}

::media::Result<MediaRealtimeCompositionGraph> MediaRealtimeCompositionGraphBuilder::buildGraph(
    const std::string& prefix, MediaRealtimeCompositionGraphOptions options)
{
    MediaGraph graph;
    auto assembly = append(graph, prefix, std::move(options));
    if (!assembly) return ::media::Result<MediaRealtimeCompositionGraph>::failure(assembly.error());
    return ::media::Result<MediaRealtimeCompositionGraph>::success({std::move(graph), std::move(assembly).value()});
}

} // namespace media::ffmpeg::graph
