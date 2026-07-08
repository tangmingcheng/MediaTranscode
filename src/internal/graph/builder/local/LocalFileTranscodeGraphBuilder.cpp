#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

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

    const MediaGraphQueueParameters& queues = options.parameters.queues;

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
    packetSelectOptions.queues = queues;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    FileOutputSegmentOptions outputOptions;
    outputOptions.prefix = "local.file";
    outputOptions.outputUrl = options.outputUrl;
    outputOptions.outputFormat = options.outputFormat;
    outputOptions.expectVideo = branchEnabled(videoPlan);
    outputOptions.expectAudio = branchEnabled(audioPlan);
    outputOptions.queues = queues;
    auto output = MediaOutputSegmentBuilder::buildFileMuxOutput(graph, outputOptions);
    if (!output) {
        return ::media::Result<MediaGraph>::failure(output.error());
    }

    MediaAudioBranchSegmentOptions audioOptions;
    audioOptions.prefix = "local.audio";
    audioOptions.plan = std::move(audioPlan);
    audioOptions.parameters = options.parameters.audio;
    audioOptions.queues = queues;
    audioOptions.formatSourceNode = input.value().input;
    audioOptions.formatSourcePort = input.value().formatPort;
    audioOptions.packetSourceNode = packetSelect.value().split;
    audioOptions.packetSourcePort = "audio";
    audioOptions.muxNode = output.value().mux;
    audioOptions.muxCodecPort = "codec";
    audioOptions.muxPacketPort = "packet";
    auto audio = MediaAudioBranchSegmentBuilder::buildIfPlanned(graph, audioOptions);
    if (!audio) {
        return ::media::Result<MediaGraph>::failure(audio.error());
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "local.video";
    videoOptions.plan = std::move(videoPlan);
    videoOptions.parameters = options.parameters.video;
    videoOptions.queues = queues;
    videoOptions.formatSourceNode = input.value().input;
    videoOptions.formatSourcePort = input.value().formatPort;
    videoOptions.packetSourceNode = packetSelect.value().split;
    videoOptions.packetSourcePort = "video";
    videoOptions.muxNode = output.value().mux;
    videoOptions.muxCodecPort = "codec";
    videoOptions.muxPacketPort = "packet";
    auto video = MediaVideoBranchSegmentBuilder::buildIfPlanned(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }

    if (!video.value()) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("LocalFileTranscodeGraphBuilder requires planned video branch"));
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
