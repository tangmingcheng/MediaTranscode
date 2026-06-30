#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineAudioSourceProbe.h"

namespace media::ffmpeg::graph {

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planFileAudio(
    const std::string& inputPath,
    const MediaAudioPipelinePlannerOptions& options)
{
    MediaAudioPipelinePlan plan;
    if (!options.includeAudio) {
        plan.reason = "disabled";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }
    if (options.transformRequested || !options.requestedCodecName.empty() ||
        options.requestedBitrateKbps || options.requestedSampleRate || options.requestedChannels) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("requested audio mode is not available"));
    }
    auto probe = MediaPipelineAudioSourceProbe::probeFile(inputPath);
    if (!probe) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(probe.error());
    }
    if (!probe.value().found) {
        plan.reason = "no_audio";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }
    plan.enabled = true;
    plan.sourceStreamIndex = probe.value().streamIndex;
    plan.sourceCodecName = probe.value().codecName;
    plan.followsSourceParameters = true;
    plan.reason = "source";
    return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
