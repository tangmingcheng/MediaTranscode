#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/local/MediaLocalFileOutputPlanner.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaPipelinePlan> videoPlanFor(const LocalFileTranscodeOptions& options)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildVideoPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<MediaPipelinePlan>::failure(plannerOptions.error());
    }
    return MediaPipelinePlanner::planVideoTranscodeFile(options.inputUrl, std::move(plannerOptions).value());
}

::media::Result<MediaAudioPipelinePlan> audioPlanFor(const LocalFileTranscodeOptions& options)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildAudioPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(plannerOptions.error());
    }
    return MediaAudioPipelinePlanner::planFileAudio(options.inputUrl, std::move(plannerOptions).value());
}

bool branchEnabled(const MediaPipelinePlan& plan) noexcept
{
    return plan.enabled && plan.branchMode != MediaBranchMode::Drop;
}

bool branchEnabled(const MediaAudioPipelinePlan& plan) noexcept
{
    return plan.enabled && plan.branchMode != MediaBranchMode::Drop;
}

} // namespace

::media::Status LocalFileTranscodeGraphBuilder::validate(const LocalFileTranscodeOptions& options)
{
    const MediaTranscodeParameterSet& parameters = options.parameters;
    if (options.inputUrl.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires inputUrl"));
    }
    if (options.outputUrl.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder requires outputUrl"));
    }
    if (parameters.queues.metadata == 0 || parameters.queues.packet == 0 ||
        parameters.queues.frame == 0 || parameters.queues.mux == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("LocalFileTranscodeGraphBuilder queue capacities must be greater than 0"));
    }
    return ::media::Status::success();
}

::media::Result<MediaGraph> LocalFileTranscodeGraphBuilder::build(const LocalFileTranscodeOptions& options)
{
    auto validation = validate(options);
    if (!validation) {
        return ::media::Result<MediaGraph>::failure(validation.error());
    }

    auto plannedVideo = videoPlanFor(options);
    if (!plannedVideo) {
        return ::media::Result<MediaGraph>::failure(plannedVideo.error());
    }
    MediaPipelinePlan videoPlan = std::move(plannedVideo).value();

    auto plannedAudio = audioPlanFor(options);
    if (!plannedAudio) {
        return ::media::Result<MediaGraph>::failure(plannedAudio.error());
    }
    MediaAudioPipelinePlan audioPlan = std::move(plannedAudio).value();

    auto outputPlan = MediaLocalFileOutputPlanner::plan(options.outputUrl);
    if (!outputPlan) {
        return ::media::Result<MediaGraph>::failure(outputPlan.error());
    }

    const MediaGraphQueueParameters& queues = options.parameters.queues;
    const MediaRealtimeEdgePolicySet edgePolicies =
        MediaBlockingEdgePolicyPlanner::plan(queues);

    MediaGraph graph;

    FileInputSegmentOptions inputOptions;
    inputOptions.prefix = "local.file";
    inputOptions.inputUrl = options.inputUrl;
    auto input = MediaInputSegmentBuilder::buildFileInput(graph, inputOptions);
    if (!input) {
        return ::media::Result<MediaGraph>::failure(input.error());
    }

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = "local";
    packetSelectOptions.formatSourceNode = input.value().input;
    packetSelectOptions.formatSourcePort = input.value().formatPort;
    packetSelectOptions.metadataPolicy = edgePolicies.metadata;
    packetSelectOptions.packetPolicy = edgePolicies.packet;
    packetSelectOptions.videoOutput = PacketSelectOutputPlan{
        videoPlan.sourceStreamIndex, MediaEdgeKind::InputPacket};
    if (branchEnabled(audioPlan)) {
        packetSelectOptions.audioOutput = PacketSelectOutputPlan{
            audioPlan.sourceStreamIndex, MediaEdgeKind::InputPacket};
    }
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    FileOutputSegmentOptions outputOptions;
    outputOptions.prefix = "local.file";
    outputOptions.outputUrl = outputPlan.value().url;
    outputOptions.outputFormat = outputPlan.value().format;
    outputOptions.outputResourceKind = outputPlan.value().outputResourceKind;
    outputOptions.expectVideo = branchEnabled(videoPlan);
    outputOptions.expectAudio = branchEnabled(audioPlan);
    outputOptions.muxSessionKind = outputPlan.value().muxSessionKind;
    outputOptions.queues = queues;
    auto output = MediaOutputSegmentBuilder::buildFileMuxOutput(graph, outputOptions);
    if (!output) {
        return ::media::Result<MediaGraph>::failure(output.error());
    }

    if (branchEnabled(audioPlan)) {
        MediaAudioBranchSegmentOptions audioOptions;
        audioOptions.prefix = "local.audio";
        audioOptions.plan = std::move(audioPlan);
        audioOptions.queues = queues;
        audioOptions.edgePolicies = edgePolicies;
        audioOptions.formatSourceNode = input.value().input;
        audioOptions.formatSourcePort = input.value().formatPort;
        audioOptions.packetSourceNode = packetSelect.value().audioPacket->node;
        audioOptions.packetSourcePort = packetSelect.value().audioPacket->port;
        audioOptions.normalizeInputPackets = true;
        audioOptions.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
        audioOptions.lineageMode = MediaAudioLineageExecutionMode::LegacyPlainPacket;
        auto audio = MediaAudioBranchSegmentBuilder::build(graph, audioOptions);
        if (!audio) {
            return ::media::Result<MediaGraph>::failure(audio.error());
        }
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, "LocalFileTranscodeGraphBuilder",
                audio.value().codec.node, audio.value().codec.port,
                output.value().mux, "codec",
                "local.audio.codec -> mux.codec", edgePolicies.metadata);
            !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, "LocalFileTranscodeGraphBuilder",
                audio.value().packet.node, audio.value().packet.port,
                output.value().mux, "packet",
                "local.audio.packet -> mux.packet", edgePolicies.audioMux);
            !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "local.video";
    videoOptions.plan = std::move(videoPlan);
    videoOptions.parameters = options.parameters.video;
    videoOptions.queues = queues;
    videoOptions.edgePolicies = edgePolicies;
    videoOptions.formatSourceNode = input.value().input;
    videoOptions.formatSourcePort = input.value().formatPort;
    videoOptions.packetSourceNode = packetSelect.value().videoPacket->node;
    videoOptions.packetSourcePort = packetSelect.value().videoPacket->port;
    videoOptions.normalizePacketCopy = true;
    auto video = MediaVideoBranchSegmentBuilder::build(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }

    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, "LocalFileTranscodeGraphBuilder",
            video.value().codec.node, video.value().codec.port,
            output.value().mux, "codec",
            "local.video.codec -> mux.codec", edgePolicies.metadata);
        !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(
            graph, "LocalFileTranscodeGraphBuilder",
            video.value().packet.node, video.value().packet.port,
            output.value().mux, "packet",
            "local.video.packet -> mux.packet", edgePolicies.videoMux);
        !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
