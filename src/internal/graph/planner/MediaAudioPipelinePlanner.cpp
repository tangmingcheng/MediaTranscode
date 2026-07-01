#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineAudioSourceProbe.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool encodeRequested(const MediaAudioPipelinePlannerOptions& options)
{
    return options.transformRequested ||
           !options.requestedCodecName.empty() ||
           !options.requestedEncoderName.empty() ||
           options.rateControl != MediaRateControlMode::Auto ||
           options.requestedBitrateKbps ||
           options.requestedMinBitrateKbps ||
           options.requestedMaxBitrateKbps ||
           options.requestedBufferSizeKbits ||
           options.requestedSampleRate ||
           options.requestedChannels ||
           options.requestedQuality ||
           !options.requestedPreset.empty() ||
           !options.requestedProfile.empty();
}

} // namespace

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planFileAudio(
    const std::string& inputPath,
    const MediaAudioPipelinePlannerOptions& options)
{
    MediaAudioPipelinePlan plan;
    if (!options.includeAudio) {
        plan.reason = "disabled";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }

    auto probe = MediaPipelineAudioSourceProbe::probeFile(inputPath);
    if (!probe) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(probe.error());
    }
    if (!probe.value().found) {
        plan.reason = "no_audio";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }

    const bool needsEncode = encodeRequested(options);
    plan.enabled = true;
    plan.sourceStreamIndex = probe.value().streamIndex;
    plan.sourceCodecName = probe.value().codecName;
    plan.mode = needsEncode ? MediaAudioPipelineMode::Encode : MediaAudioPipelineMode::Copy;
    plan.followsSourceParameters = !needsEncode;
    plan.targetCodecName = options.requestedCodecName.empty() ? plan.sourceCodecName : options.requestedCodecName;
    plan.targetEncoderName = options.requestedEncoderName;
    plan.reason = needsEncode ? "encode" : "copy";
    return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
