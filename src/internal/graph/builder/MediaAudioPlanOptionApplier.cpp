#include "internal/graph/builder/MediaAudioPlanOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioPlanOptionApplier";

::media::Result<void> setOption(MediaGraph& graph,
                                 MediaNodeId nodeId,
                                 const std::string& key,
                                 const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

} // namespace

::media::Result<void> MediaAudioPlanOptionApplier::applySelectedPlan(
    MediaGraph& graph,
    const MediaAudioEncodeBranchNodes& nodes,
    const MediaAudioPipelinePlan& plan)
{
    if (plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioPlanOptionApplier requires transcode_frame audio branch"));
    }
    if (plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioPlanOptionApplier requires planned audio source stream index"));
    }

    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph,
                                                                     owner,
                                                                     nodes.packetNormalize,
                                                                     MediaStreamKind::Audio,
                                                                     plan.sourceStreamIndex); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioSourceStreamIndex, std::to_string(plan.sourceStreamIndex)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioCodec, plan.targetCodecName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, plan.targetEncoderName); !status) return status;
    return setOption(graph, nodes.encode, MediaTranscodeOptionKey::PlannedEncoder, plan.targetEncoderName);
}

} // namespace media::ffmpeg::graph
