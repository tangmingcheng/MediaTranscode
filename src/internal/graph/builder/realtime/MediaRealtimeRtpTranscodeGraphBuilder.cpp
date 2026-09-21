#include "internal/graph/builder/realtime/MediaRealtimeInputGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputSourceRetentionPlanner.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeVideoEncodingGroupBuilder.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeVideoSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

#include <algorithm>
#include <string>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

::media::Status appendVideoProtocolOutput(
    MediaGraph& graph, const std::string& prefix,
    const MediaEncodedBranchEndpoints& video,
    const MediaRealtimeVideoRuntimePlan& runtime)
{
    MediaRealtimeVideoSchedulerSegmentOptions options;
    options.prefix = prefix + ".output";
    options.encodedVideo = video.packet;
    auto scheduled = MediaRealtimeVideoSchedulerSegmentBuilder::build(graph, options, runtime);
    if (!scheduled) return ::media::Status::failure(scheduled.error());
    if (std::holds_alternative<MediaVideoOnlySeparateRtpOutputRuntimePlan>(runtime.outputAdapter)) {
        auto output = MediaScheduledRtpOutputSegmentBuilder::buildVideoOnly(
            graph, MediaVideoOnlyScheduledRtpOutputSegmentOptions{
                prefix + ".rtp_output", scheduled.value().activation,
                video.codec, scheduled.value().scheduledVideo}, runtime);
        return output ? ::media::Status::success() : ::media::Status::failure(output.error());
    }
    if (std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(runtime.outputAdapter)) {
        auto output = MediaScheduledMpegTsOutputSegmentBuilder::buildVideoOnly(
            graph, MediaVideoOnlyScheduledMpegTsOutputSegmentOptions{
                prefix + ".mpegts_output", scheduled.value().activation,
                video.codec, scheduled.value().scheduledVideo}, runtime);
        return output ? ::media::Status::success() : ::media::Status::failure(output.error());
    }
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "video output requires a production protocol adapter"));
}

::media::Result<MediaNodeId> findPreparedInputTarget(
    const MediaGraph& graph,
    MediaPreparedRealtimeInputKind expectedKind,
    std::optional<std::string_view> rawRtpStream)
{
    if ((expectedKind == MediaPreparedRealtimeInputKind::RawRtp) !=
        rawRtpStream.has_value()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared input target requires an exact kind and stream identity"));
    }
    MediaNodeId target = MediaNodeId::invalid();
    for (const MediaNode& node : graph.nodes()) {
        const bool matches = rawRtpStream
            ? node.kind == MediaNodeKind::RawRtpInput &&
                node.options.has("rtp.stream_kind") &&
                node.options.value("rtp.stream_kind") == *rawRtpStream
            : node.kind == MediaNodeKind::RealtimeInput;
        if (!matches) continue;
        if (target.isValid()) {
            return ::media::Result<MediaNodeId>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "realtime executable graph has duplicate prepared input targets"));
        }
        target = node.id;
    }
    if (!target.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph has no prepared input target"));
    }
    return ::media::Result<MediaNodeId>::success(target);
}

} // namespace

::media::Status MediaRealtimeRtpTranscodeGraphBuilder::validate(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    return MediaRealtimeRtpTranscodePlanner::validateRealtimeRequestNoIo(options);
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.type || *options.input.type != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported(
                "URL and MPEG-TS realtime input require preflight() and buildExecutable()"));
    }
    auto realtimePlan = MediaRealtimeRtpTranscodePlanner::plan(options);
    if (!realtimePlan) {
        return ::media::Result<MediaGraph>::failure(realtimePlan.error());
    }
    return build(std::move(realtimePlan).value());
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    MediaRealtimeRtpTranscodePlan plan)
{
    std::optional<MediaAvRuntimeRegistrationPlan> registration;
    return buildPlanned(plan, registration);
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::buildPlanned(
    MediaRealtimeRtpTranscodePlan& plan,
    std::optional<MediaAvRuntimeRegistrationPlan>& registration)
{
    if (auto status = MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
            plan); !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    MediaGraph graph;

    const auto* videoRuntime = std::get_if<MediaRealtimeVideoRuntimePlan>(&plan.runtime);
    const auto* avRuntime = std::get_if<MediaRealtimeAvSyncRuntimePlan>(&plan.runtime);
    const auto& queues = videoRuntime ? videoRuntime->queues : avRuntime->queues;
    const auto& edgePolicies = videoRuntime ? videoRuntime->edgePolicies : avRuntime->edgePolicies;
    auto inputs = MediaRealtimeInputGraphBuilder::append(graph, "realtime", plan);
    if (!inputs) return ::media::Result<MediaGraph>::failure(inputs.error());
    auto& synchronizedInput = inputs.value().synchronized;
    const bool videoPacketNormalization = videoRuntime && videoRuntime->packetCopyNormalizationRequired;
    const bool audioBranchEnabled = avRuntime != nullptr;
    registration.reset();
    if (avRuntime) {
        registration.emplace();
        registration->input = synchronizedInput->registration;
        registration->processing.source = inputs.value().sourceMembers;
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "realtime.video";
    videoOptions.plan = std::move(plan.videoPlan);
    videoOptions.parameters = plan.videoParameters;
    videoOptions.queues = queues;
    videoOptions.edgePolicies = edgePolicies;
    if (videoRuntime) {
        videoOptions.lineageEdgePolicies =
            videoRuntime->lineageEdgePolicies;
    }
    if (avRuntime) {
        videoOptions.edgePolicies.videoPacket =
            edgePolicies.startupVideoRelease;
    }
    videoOptions.inputStartRequiresKeyFrame = synchronizedInput
        ? false : plan.videoInputStartRequiresKeyFrame;
    if (avRuntime) {
        if (!avRuntime->synchronization.startup
                 .requireVideoKeyFrame) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized realtime video requires planner generation-start key-frame policy"));
        }
        if (avRuntime->queues.frame == 0) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized realtime video requires positive planned lineage capacity"));
        }
        videoOptions.canonicalLineageCapacity =
            avRuntime->queues.frame;
        videoOptions.generationStartRequiresKeyFrame =
            *avRuntime->synchronization.startup
                 .requireVideoKeyFrame;
    }
    videoOptions.formatSourceNode = inputs.value().videoFormat.node;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = inputs.value().videoPacket.node;
    videoOptions.packetSourcePort = inputs.value().videoPacket.port;
    videoOptions.normalizePacketCopy = videoPacketNormalization;
    auto video = MediaVideoBranchSegmentBuilder::build(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }

    if (avRuntime) {
        if (!video.value().startupPreparationOwner) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized video branch lacks its preparation owner product"));
        }
        registration->preparationOwner = *video.value().startupPreparationOwner;
        registration->processing.append(video.value().processing);
    }
    std::optional<MediaEncodedBranchEndpoints> audio;
    if (audioBranchEnabled) {
        const auto& avSyncRuntime = *avRuntime;
        MediaAudioBranchSegmentOptions audioOptions;
        audioOptions.prefix = "realtime.audio";
        audioOptions.plan = std::move(avRuntime->audioPipeline);
        audioOptions.queues = queues;
        audioOptions.edgePolicies = edgePolicies;
        audioOptions.edgePolicies.audioPacket =
            edgePolicies.startupAudioRelease;
        audioOptions.formatSourceNode = inputs.value().audioFormat.node;
        audioOptions.formatSourcePort = "format";
        audioOptions.packetSourceNode = inputs.value().audioPacket.node;
        audioOptions.packetSourcePort = inputs.value().audioPacket.port;
        audioOptions.normalizeInputPackets = false;
        if (auto status = mapSynchronizedAudioBranchOptions(
                avSyncRuntime, audioOptions); !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
        auto builtAudio = MediaAudioBranchSegmentBuilder::build(
            graph, audioOptions);
        if (!builtAudio) {
            return ::media::Result<MediaGraph>::failure(builtAudio.error());
        }
        audio = std::move(builtAudio).value();
        registration->processing.append(audio->processing);
    }

    if (avRuntime) {
        if (!synchronizedInput || !audio) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized realtime output requires complete A/V endpoints"));
        }
        MediaRealtimeAvSchedulerSegmentOptions schedulerOptions;
        schedulerOptions.prefix = "realtime.av_sync.output";
        schedulerOptions.canonicalVideo = video.value().packet;
        schedulerOptions.canonicalAudio = audio->packet;
        auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(
            graph, schedulerOptions, *avRuntime);
        if (!scheduled) {
            return ::media::Result<MediaGraph>::failure(scheduled.error());
        }
        registration->outputScheduler = scheduled.value().scheduler;
        registration->processing.output.insert(registration->processing.output.end(),
            scheduled.value().outputMembers.begin(), scheduled.value().outputMembers.end());
        if (avRuntime->outputAdapter ==
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
            MediaScheduledRtpOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.rtp_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduledVideo = scheduled.value().video;
            outputOptions.scheduledAudio = scheduled.value().audio;
            auto output = MediaScheduledRtpOutputSegmentBuilder::build(
                graph, outputOptions, *avRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
            registration->processing.output.insert(registration->processing.output.end(),
                output.value().outputMembers.begin(), output.value().outputMembers.end());
        } else if (avRuntime->outputAdapter ==
                   MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
            MediaScheduledMpegTsOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.mpegts_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduled = scheduled.value().serialized;
            outputOptions.expectVideo =
                videoOptions.plan.enabled && videoOptions.plan.branchMode != MediaBranchMode::Drop;
            outputOptions.expectAudio = true;
            auto output = MediaScheduledMpegTsOutputSegmentBuilder::build(
                graph, outputOptions, *avRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
            registration->processing.output.insert(registration->processing.output.end(),
                output.value().outputMembers.begin(), output.value().outputMembers.end());
            if (output.value().rtpSdpPublisher.isValid())
                registration->rtpSdpPublisher = output.value().rtpSdpPublisher;
        } else {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "Synchronized realtime output requires a production adapter"));
        }
    } else if (videoRuntime) {
        if (!videoOptions.plan.encodedOutputFanout) return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::notInitialized("Initial video output lacks an encoding group fanout plan"));
        auto encoded = MediaRealtimeVideoEncodingGroupBuilder::appendFanout(graph,
            "realtime.video", video.value(), *videoOptions.plan.encodedOutputFanout,
            videoRuntime->edgePolicies.synchronizedPacket);
        if (!encoded) return ::media::Result<MediaGraph>::failure(encoded.error());
        if (auto status = appendVideoProtocolOutput(
                graph, "realtime.video_only", encoded.value(), *videoRuntime); !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
    } else {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime graph lost its typed runtime variant"));
    }

    if (!plan.resourceLedger) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "final realtime graph requires its resource planning ledger"));
    }
    if (plan.videoPlan.sourcePlaybackEpoch) {
        if (plan.videoPlan.sourcePlaybackEpoch->identityTransition !=
            MediaVideoSourceIdentityTransition::ReplanSession) {
            return ::media::Result<MediaGraph>::failure(::media::ErrorInfo::unsupported(
                "unsupported shared video source identity transition"));
        }
        for (const auto& node : graph.nodes()) {
            if (node.kind != MediaNodeKind::RawRtpInput) continue;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner,
                    node.id, "source.playback_epoch.identity_transition", "replan_session"); !status) {
                return ::media::Result<MediaGraph>::failure(status.error());
            }
        }
    }
    auto finalLedger = MediaFinalGraphResourceLedgerCompiler::compile(
        graph, *plan.resourceLedger, {});
    if (!finalLedger) {
        return ::media::Result<MediaGraph>::failure(finalLedger.error());
    }
    MediaNodeId codecResolver = MediaNodeId::invalid();
    for (const auto& node : graph.nodes()) {
        if (node.kind != MediaNodeKind::CodecResolver) continue;
        if (codecResolver.isValid()) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "final realtime graph has duplicate video codec resolvers"));
        }
        codecResolver = node.id;
    }
    if (!codecResolver.isValid()) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "final realtime graph lacks its video codec resolver"));
    }
    const auto& finalized = finalLedger.value();
    if (!graph.setPayloadCreditPlan(finalized.payloadCreditPlan)) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime graph rejected its complete payload credit plan"));
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, codecResolver,
            "resource.graph_payload_reserved_bytes",
            std::to_string(
                finalized.admittedGraphPayloadAndReservedStorageBytes));
        !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, codecResolver,
            "resource.observed_external_allocation",
            finalized.outOfScopeAuthorities.empty() ? "0" : "1");
        !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    if (finalized.encoderFramesPool) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, codecResolver,
                "encoder.hardware_frames.initial_pool_surfaces",
                std::to_string(
                    finalized.encoderFramesPool->initialPoolSurfaces));
            !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, codecResolver,
                "encoder.hardware_frames.pool_authority",
                finalized.encoderFramesPool->authority);
            !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
    }
    std::visit([&](auto& runtime) {
        runtime.threadingPolicy.maxWorkerThreads = graph.nodeCount();
    }, plan.runtime);
    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaRealtimeVideoEncodingGroupGraph>
MediaRealtimeRtpTranscodeGraphBuilder::appendEncodingGroup(
    MediaGraph graph,
    const MediaRealtimeRtpTranscodePlan& plan,
    const std::string& prefix,
    MediaEndpoint formatSource,
    MediaSharedVideoDecodeEndpoints sharedDecode,
    const MediaRuntimeReclamationPlan& reclamationPlan)
{
    using Result = ::media::Result<MediaRealtimeVideoEncodingGroupGraph>;
    const auto* runtime = std::get_if<MediaRealtimeVideoRuntimePlan>(&plan.runtime);
    if (!runtime || !plan.resourceLedger || prefix.empty() || !graph.payloadCreditPlan()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "output branch requires a video runtime, resource ledger, prefix and admitted session graph"));
    }
    for (const auto& node : graph.nodes()) {
        if (node.name.starts_with(prefix + ".")) return Result::failure(
            ::media::ErrorInfo::invalidArgument("output branch prefix is already allocated"));
    }
    const auto previousNodes = graph.nodeCount();
    auto* sourceCodec = graph.findOutputPort(sharedDecode.codec.node, sharedDecode.codec.port);
    auto* sourceFormat = graph.findOutputPort(formatSource.node, formatSource.port);
    auto* sourceFrame = graph.findOutputPort(sharedDecode.frame.node, sharedDecode.frame.port);
    if (!sourceCodec || !sourceFormat || !sourceFrame ||
        sourceCodec->payloadKind != MediaPayloadKind::CodecContext ||
        sourceFrame->payloadKind != MediaPayloadKind::Frame) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "output branch requires existing decoded frame and metadata endpoints"));
    }
    sourceCodec->multiple = true;
    sourceFormat->multiple = true;
    sourceFrame->multiple = true;
    MediaVideoTranscodeBranchOptions options;
    options.prefix = prefix;
    options.plan = plan.videoPlan;
    options.parameters = plan.videoParameters;
    options.queues = runtime->queues;
    options.edgePolicies = runtime->edgePolicies;
    options.lineageEdgePolicies = runtime->lineageEdgePolicies;
    options.formatSourceNode = formatSource.node;
    options.formatSourcePort = formatSource.port;
    options.sharedDecode = std::move(sharedDecode);
    auto video = MediaVideoTranscodeBranchBuilder::build(graph, options);
    if (!video) return Result::failure(video.error());
    if (!plan.videoPlan.encodedOutputFanout) return Result::failure(
        ::media::ErrorInfo::notInitialized("Encoding group requires its planned packet fanout"));
    auto encoded = MediaRealtimeVideoEncodingGroupBuilder::appendFanout(graph, prefix,
        video.value(), *plan.videoPlan.encodedOutputFanout, runtime->edgePolicies.synchronizedPacket);
    if (!encoded) return Result::failure(encoded.error());
    std::vector<MediaNodeId> nodes;
    for (std::size_t index = previousNodes; index < graph.nodeCount(); ++index) {
        nodes.push_back(graph.nodes()[index].id);
    }
    auto resources = MediaFinalGraphResourceLedgerCompiler::compile(
        graph, *plan.resourceLedger, nodes);
    if (!resources) return Result::failure(resources.error());
    auto* resolver = graph.findNode(nodes.front());
    if (!resolver || resolver->kind != MediaNodeKind::CodecResolver) return Result::failure(
        ::media::ErrorInfo::internalError("output branch lost its prepared encoder resolver"));
    if (resources.value().encoderFramesPool) {
        resolver->options.set("encoder.hardware_frames.initial_pool_surfaces",
            std::to_string(resources.value().encoderFramesPool->initialPoolSurfaces));
        resolver->options.set("encoder.hardware_frames.pool_authority",
            resources.value().encoderFramesPool->authority);
    }
    // Keep existing producers' contracts. The session manager admits aggregate
    // limits before publishing this immutable description to any worker.
    auto merged = *graph.payloadCreditPlan();
    const auto& added = resources.value().payloadCreditPlan;
    merged.producers.insert(merged.producers.end(), added.producers.begin(), added.producers.end());
    merged.maximumUnitBytes = (std::max)(merged.maximumUnitBytes, added.maximumUnitBytes);
    if (!graph.replacePayloadCreditPlan(std::move(merged))) return Result::failure(
        ::media::ErrorInfo::invalidArgument("output branch producer contract merge failed"));
    auto growth = MediaRealtimeOutputSourceRetentionPlanner::plan(
        graph, nodes, options.sharedDecode->frame.node, *plan.resourceLedger, resources.value());
    if (!growth) return Result::failure(growth.error());
    auto threading = runtime->threadingPolicy;
    threading.maxWorkerThreads = nodes.size();
    return Result::success(MediaRealtimeVideoEncodingGroupGraph{std::move(graph),
        {std::move(nodes), threading, std::move(resources).value(),
         std::move(growth).value(), std::move(encoded).value(), reclamationPlan}});
}

::media::Result<MediaRealtimeVideoProtocolOutputGraph>
MediaRealtimeRtpTranscodeGraphBuilder::appendProtocolOutput(
    MediaGraph graph, const MediaRealtimeRtpTranscodePlan& plan,
    const std::string& prefix, MediaEncodedBranchEndpoints encoded,
    const MediaRealtimeVideoJoinWaitPlan& joinWaitPlan,
    const MediaRuntimeReclamationPlan& reclamationPlan)
{
    using Result = ::media::Result<MediaRealtimeVideoProtocolOutputGraph>;
    const auto* runtime = std::get_if<MediaRealtimeVideoRuntimePlan>(&plan.runtime);
    const auto* fanout = graph.findNode(encoded.packet.node);
    if (!runtime || !plan.resourceLedger || !fanout ||
        fanout->kind != MediaNodeKind::EncodedVideoOutputFanout || !graph.payloadCreditPlan())
        return Result::failure(::media::ErrorInfo::notInitialized("Protocol output requires an admitted encoding group"));
    const auto previous = graph.nodeCount();
    if (auto status = appendVideoProtocolOutput(graph, prefix, encoded, *runtime); !status)
        return Result::failure(status.error());
    std::vector<MediaNodeId> nodes;
    for (std::size_t index = previous; index < graph.nodeCount(); ++index) nodes.push_back(graph.nodes()[index].id);
    auto storage = MediaFinalGraphResourceLedgerCompiler::compileReferenceStorage(graph, *plan.resourceLedger, nodes);
    if (!storage) return Result::failure(storage.error());
    MediaNodeId producer;
    for (const auto& edge : graph.edges()) {
        if (edge.to.nodeId == encoded.packet.node && edge.payloadKind == MediaPayloadKind::Packet) producer = edge.from.nodeId;
    }
    const auto& strategies = graph.payloadCreditPlan()->producers;
    if (std::none_of(strategies.begin(), strategies.end(), [&](const auto& strategy) {
            return strategy.nodeId == producer && strategy.payloadKind == MediaPayloadKind::Packet;
        })) return Result::failure(::media::ErrorInfo::notInitialized("Encoded fanout has no packet allocation producer"));
    std::uint64_t references = 0;
    std::vector<MediaNodeId> packetConsumers;
    for (const auto& edge : graph.edges()) {
        if (edge.payloadKind != MediaPayloadKind::Packet ||
            std::find(nodes.begin(), nodes.end(), edge.to.nodeId) == nodes.end()) continue;
        auto count = MediaCheckedArithmetic::add(references, edge.policy.queuePolicy.capacity,
            "protocol retained packet queue references");
        if (!count) return Result::failure(count.error());
        references = count.value();
        if (std::find(packetConsumers.begin(), packetConsumers.end(), edge.to.nodeId) == packetConsumers.end())
            packetConsumers.push_back(edge.to.nodeId);
    }
    auto active = MediaCheckedArithmetic::add(references, packetConsumers.size(), "protocol active packet input slots");
    auto count = active ? MediaCheckedArithmetic::add(active.value(), runtime->startup.packetCapacity,
        "protocol scheduler prepared startup retention slots") : active;
    if (!count || count.value() == 0) return Result::failure(count ?
        ::media::ErrorInfo::invalidArgument("Protocol retention is empty") : count.error());
    auto threading = runtime->threadingPolicy;
    threading.maxWorkerThreads = nodes.size();
    return Result::success({std::make_shared<const MediaGraph>(std::move(graph)),
        std::move(nodes), threading, storage.value().reservedStorageBytes,
        {producer, plan.resourceLedger->media.videoBytes, count.value()}, joinWaitPlan, reclamationPlan});
}

::media::Result<MediaRealtimeInitialVideoOutputTopology>
MediaRealtimeRtpTranscodeGraphBuilder::initialOutputTopology(const MediaGraph& graph)
{
    using Result = ::media::Result<MediaRealtimeInitialVideoOutputTopology>;
    MediaRealtimeInitialVideoOutputTopology topology;
    MediaNodeId packetFanout;
    for (const auto& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoOutputFanout) {
            if (topology.sourceFanout.isValid()) return Result::failure(::media::ErrorInfo::invalidArgument("Duplicate initial frame fanout"));
            topology.sourceFanout = node.id;
        }
        if (node.kind == MediaNodeKind::EncodedVideoOutputFanout) {
            if (packetFanout.isValid()) return Result::failure(::media::ErrorInfo::invalidArgument("Duplicate initial encoding group"));
            packetFanout = node.id;
        }
    }
    if (!topology.sourceFanout.isValid() || !packetFanout.isValid()) return Result::failure(
        ::media::ErrorInfo::notInitialized("Initial video topology requires frame and encoded distributors"));
    const auto dependencies = [&](MediaNodeId root) {
        std::vector<MediaNodeId> ids{root};
        for (std::size_t i = 0; i < ids.size(); ++i) {
            for (const auto& edge : graph.edges()) {
                if (edge.to.nodeId == ids[i] && std::find(ids.begin(), ids.end(), edge.from.nodeId) == ids.end()) ids.push_back(edge.from.nodeId);
            }
        }
        return ids;
    };
    topology.sharedNodeIds = dependencies(topology.sourceFanout);
    topology.encodingNodeIds = dependencies(packetFanout);
    std::erase_if(topology.encodingNodeIds, [&](MediaNodeId node) {
        return std::find(topology.sharedNodeIds.begin(), topology.sharedNodeIds.end(), node) != topology.sharedNodeIds.end();
    });
    for (const auto& node : graph.nodes()) {
        if (std::find(topology.sharedNodeIds.begin(), topology.sharedNodeIds.end(), node.id) == topology.sharedNodeIds.end() &&
            std::find(topology.encodingNodeIds.begin(), topology.encodingNodeIds.end(), node.id) == topology.encodingNodeIds.end())
            topology.outputNodeIds.push_back(node.id);
    }
    for (const auto& edge : graph.edges()) {
        if (edge.to.nodeId == packetFanout && edge.payloadKind == MediaPayloadKind::Packet)
            topology.encoded = {{edge.from.nodeId, "codec_parameters"}, {packetFanout, "packet"}};
    }
    if (!topology.encoded.codec.valid()) return Result::failure(::media::ErrorInfo::notInitialized("Initial encoding metadata endpoint is absent"));
    return Result::success(std::move(topology));
}

::media::Result<MediaRealtimeExecutableGraph> MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
    MediaRealtimeTranscodePreflight preflight)
{
    if (auto status = MediaRealtimeTsInputPlanValidator::validate(
            preflight.plan.inputType, preflight.plan.input); !status) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(status.error());
    }
    const RealtimeInputType inputType = preflight.plan.inputType;
    const auto requiredPreparedKind =
        preflight.plan.requiredPreparedInputKind;
    const bool requiresPrepared = requiredPreparedKind.has_value();
    if (!requiresPrepared && inputType != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "non-RTP realtime executable graph requires prepared input"));
    }
    if (!requiresPrepared && preflight.prepared) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "node-owned raw RTP plan rejects an unplanned prepared input"));
    }
    if (requiresPrepared && (!preflight.prepared || !preflight.prepared->valid() ||
        preflight.prepared->kind() != requiredPreparedKind)) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph requires the exact planner-selected prepared input"));
    }
    bool requiresPreparedAudio = false;
    const auto* avRuntime = std::get_if<MediaRealtimeAvSyncRuntimePlan>(
        &preflight.plan.runtime);
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput =
        avRuntime && avRuntime->isolatedAudioInput
        ? &*avRuntime->isolatedAudioInput
        : nullptr;
    if (isolatedAudioInput) {
        if (!isolatedAudioInput->requiresPreparedInput) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated realtime audio input requires an explicit prepared-input ownership decision"));
        }
        requiresPreparedAudio =
            *isolatedAudioInput->requiresPreparedInput;
    }
    if (requiresPreparedAudio &&
        (!preflight.preparedAudio || !preflight.preparedAudio->valid() ||
         preflight.preparedAudio->kind() !=
             MediaPreparedRealtimeInputKind::RawRtp)) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph requires synchronized prepared RTP audio input"));
    }
    if (!requiresPreparedAudio && preflight.preparedAudio) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime executable graph rejects unplanned prepared audio input"));
    }
    std::optional<MediaAvRuntimeRegistrationPlan> registration;
    auto graphResult = buildPlanned(preflight.plan, registration);
    if (!graphResult) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            graphResult.error());
    }

    MediaRealtimeRuntimeBinding runtimeBinding;
    if (auto* videoRuntime = std::get_if<MediaRealtimeVideoRuntimePlan>(
            &preflight.plan.runtime)) {
        runtimeBinding.emplace<MediaRealtimeVideoRuntimeBinding>(
            MediaRealtimeVideoRuntimeBinding{
                std::move(*videoRuntime),
                std::move(preflight.plan.input.rtpTransport)});
    } else if (auto* runtimePlan =
                   std::get_if<MediaRealtimeAvSyncRuntimePlan>(
                       &preflight.plan.runtime)) {
        const auto audioExecutionProduct =
            std::holds_alternative<MediaSynchronizedAudioPacketCopyBounds>(
                runtimePlan->componentBounds)
                ? MediaSynchronizedAudioExecutionProduct::PacketCopy
                : MediaSynchronizedAudioExecutionProduct::FrameTranscode;
        auto outputProduct = std::visit(
            []<typename Product>(Product&& product)
                -> MediaAvSyncRuntimeOutputProduct {
                return MediaAvSyncRuntimeOutputProduct(
                    std::forward<Product>(product));
            },
            std::move(runtimePlan->protocolOutput));
        const auto outputGroupKey = runtimePlan->groupKey;
        std::vector<MediaAvRuntimeDomainBinding> domains;
        domains.push_back(MediaAvRuntimeDomainBinding{
            std::move(runtimePlan->groupKey),
            std::move(runtimePlan->synchronization),
            MediaAvSharedSourceOutputDomainBinding{
                std::move(runtimePlan->transition), std::move(*registration), nullptr}});
        runtimeBinding.emplace<MediaAvSyncRuntimeBinding>(
            MediaAvSyncRuntimeBinding{
            std::move(domains),
            outputGroupKey,
            runtimePlan->edgePolicies,
            std::move(runtimePlan->datagramTransport),
            audioExecutionProduct,
            std::move(outputProduct)});
    } else {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime executable graph requires a typed runtime binding"));
    }
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graphResult).value();
    executable.runtimeBinding = std::move(runtimeBinding);
    if (requiresPrepared) {
        auto inputId = findPreparedInputTarget(
            executable.graph, *requiredPreparedKind,
            *requiredPreparedKind == MediaPreparedRealtimeInputKind::RawRtp
                ? std::optional<std::string_view>("video")
                : std::nullopt);
        if (!inputId) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                inputId.error());
        }
        executable.inputBindings.push_back(
            MediaPreparedRealtimeInputBinding{
                inputId.value(),
                *requiredPreparedKind,
                std::move(*preflight.prepared)});
    }
    if (requiresPreparedAudio) {
        auto audioInputId = findPreparedInputTarget(
            executable.graph, MediaPreparedRealtimeInputKind::RawRtp,
            std::optional<std::string_view>("audio"));
        if (!audioInputId) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                audioInputId.error());
        }
        executable.inputBindings.push_back(
            MediaPreparedRealtimeInputBinding{
                audioInputId.value(),
                MediaPreparedRealtimeInputKind::RawRtp,
                std::move(*preflight.preparedAudio)});
    }
    return ::media::Result<MediaRealtimeExecutableGraph>::success(std::move(executable));
}

} // namespace media::ffmpeg::graph
