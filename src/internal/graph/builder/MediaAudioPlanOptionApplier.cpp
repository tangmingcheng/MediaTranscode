#include "internal/graph/builder/MediaAudioPlanOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioPlanOptionApplier";

::media::Result<void> setOption(MediaGraph& graph, MediaNodeId nodeId,
                                const std::string& key, const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setOptional(MediaGraph& graph, MediaNodeId nodeId,
                                  const std::string& key, const std::optional<int>& value)
{
    return value ? setOption(graph, nodeId, key, std::to_string(*value))
                 : ::media::Result<void>::success();
}

std::string plannedProfile(const MediaAudioProfile& profile)
{
    switch (profile.knowledge()) {
    case MediaAudioProfileKnowledge::Known: return profile.canonicalName();
    case MediaAudioProfileKnowledge::NotApplicable: return "not_applicable";
    case MediaAudioProfileKnowledge::Unknown: return "unknown";
    }
    return {};
}

} // namespace

::media::Result<void> MediaAudioPlanOptionApplier::applySelectedPlan(
    MediaGraph& graph, const MediaAudioEncodeBranchNodes& nodes,
    const MediaAudioPipelinePlan& plan, bool normalizePackets)
{
    if (plan.branchMode != MediaBranchMode::TranscodeFrame || !plan.resolvedOutput ||
        plan.resolvedOutput->branchMode() != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaAudioPlanOptionApplier requires complete transcode audio plan"));
    }
    if (plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioPlanOptionApplier requires planned audio source stream index"));
    }
    const auto& output = *plan.resolvedOutput;
    if (normalizePackets) {
        if (auto status = MediaGraphBuildSupport::setPacketNormalizeOptions(
                graph, owner, nodes.packetNormalize, MediaStreamKind::Audio,
                plan.sourceStreamIndex, false); !status) return status;
    }
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioSourceStreamIndex, std::to_string(plan.sourceStreamIndex)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioCodec, output.codecName()); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, output.encoderName()); !status) return status;
    if (auto status = setOption(graph, nodes.encode, MediaTranscodeOptionKey::PlannedEncoder, output.encoderName()); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioRateControl, mediaRateControlModeName(output.rateControl())); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioSampleRate, std::to_string(output.sampleRate())); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioChannels, std::to_string(output.channels())); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioChannelLayout, output.channelLayout()); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioSampleFormat, output.sampleFormat()); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioProfile, plannedProfile(output.profile())); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioProfileId, std::to_string(output.profile().ffmpegProfileId())); !status) return status;
    if (auto status = setOptional(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioBitrateKbps, output.bitrateKbps()); !status) return status;
    if (auto status = setOptional(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioMinBitrateKbps, output.minBitrateKbps()); !status) return status;
    if (auto status = setOptional(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioMaxBitrateKbps, output.maxBitrateKbps()); !status) return status;
    if (auto status = setOptional(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioBufferSizeKbits, output.bufferSizeKbits()); !status) return status;
    if (auto status = setOptional(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioQuality, output.quality()); !status) return status;
    if (!output.preset().empty()) return setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::AudioPreset, output.preset());
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
