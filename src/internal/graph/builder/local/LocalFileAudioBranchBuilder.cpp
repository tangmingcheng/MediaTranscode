#include "internal/graph/builder/local/LocalFileAudioBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> setMuxExpectAudio(MediaGraph& graph, MediaNodeId mux, bool expectAudio)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                        "LocalFileAudioBranchBuilder",
                                                        mux,
                                                        MediaTranscodeOptionKey::MuxExpectAudio,
                                                        expectAudio ? "1" : "0");
}

} // namespace

::media::Result<bool> LocalFileAudioBranchBuilder::buildIfPlanned(MediaGraph& graph,
                                                                  const LocalFileTranscodeOptions& options,
                                                                  MediaNodeId fileInput,
                                                                  MediaNodeId split,
                                                                  MediaNodeId mux)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildAudioPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<bool>::failure(plannerOptions.error());
    }

    auto planResult = MediaAudioPipelinePlanner::planFileAudio(options.inputUrl, std::move(plannerOptions).value());
    if (!planResult) {
        return ::media::Result<bool>::failure(planResult.error());
    }

    MediaAudioPipelinePlan plan = std::move(planResult).value();
    const bool buildAudioBranch = plan.enabled && plan.branchMode != MediaBranchMode::Drop;
    if (auto status = setMuxExpectAudio(graph, mux, buildAudioBranch); !status) {
        return ::media::Result<bool>::failure(status.error());
    }
    if (!buildAudioBranch) {
        return ::media::Result<bool>::success(false);
    }

    MediaAudioBranchSegmentOptions branchOptions;
    branchOptions.prefix = "local.audio";
    branchOptions.plan = std::move(plan);
    branchOptions.parameters = options.parameters.audio;
    branchOptions.queues = options.parameters.queues;
    branchOptions.formatSourceNode = fileInput;
    branchOptions.formatSourcePort = "format";
    branchOptions.packetSourceNode = split;
    branchOptions.packetSourcePort = "audio";
    branchOptions.muxNode = mux;
    branchOptions.muxCodecPort = "codec";
    branchOptions.muxPacketPort = "packet";

    return MediaAudioBranchSegmentBuilder::buildIfPlanned(graph, branchOptions);
}

} // namespace media::ffmpeg::graph
