#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelineAudioSourceProbe.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string canonicalAudioCodecName(std::string codec)
{
    codec = lowerCopy(std::move(codec));
    if (codec == "mp4a" || codec == "mpeg4aac" || codec == "aac_lc") {
        return "aac";
    }
    return codec;
}

template <typename T>
bool targetChanged(const std::optional<T>& requested, T source)
{
    return requested.has_value() && *requested != source;
}

bool stringTargetChanged(const std::string& requested, const std::string& source)
{
    return !requested.empty() && lowerCopy(requested) != lowerCopy(source);
}

bool requiresEncodeForUnobservableEncoderOption(const MediaAudioPipelinePlannerOptions& options)
{
    return options.rateControl != MediaRateControlMode::Auto ||
           options.requestedMinBitrateKbps ||
           options.requestedMaxBitrateKbps ||
           options.requestedBufferSizeKbits ||
           options.requestedQuality ||
           !options.requestedPreset.empty();
}

bool sourceMatchesRequestedTarget(const MediaPipelineAudioSourceProbeResult& source,
                                  const MediaAudioPipelinePlannerOptions& options)
{
    const MediaFormatDescriptor& descriptor = source.descriptor;
    const std::string sourceCodec = canonicalAudioCodecName(source.codecName);
    const std::string targetCodec = canonicalAudioCodecName(options.requestedCodecName.empty() ? sourceCodec : options.requestedCodecName);
    if (targetCodec != sourceCodec) {
        return false;
    }

    if (targetChanged(options.requestedSampleRate, descriptor.audio.sampleRate)) {
        return false;
    }
    if (targetChanged(options.requestedChannels, descriptor.audio.channels)) {
        return false;
    }
    if (options.requestedBitrateKbps) {
        const int sourceKbps = descriptor.codec.bitrate > 0 ? static_cast<int>(descriptor.codec.bitrate / 1000) : 0;
        if (sourceKbps <= 0 || *options.requestedBitrateKbps != sourceKbps) {
            return false;
        }
    }
    if (stringTargetChanged(options.requestedProfile, descriptor.codec.profile)) {
        return false;
    }
    if (requiresEncodeForUnobservableEncoderOption(options)) {
        return false;
    }

    return true;
}

const AVCodec* findAudioEncoderForCodecName(const std::string& codecName)
{
    if (codecName.empty()) {
        return nullptr;
    }
    if (const AVCodec* direct = avcodec_find_encoder_by_name(codecName.c_str())) {
        return direct;
    }
    const AVCodecDescriptor* descriptor = avcodec_descriptor_get_by_name(codecName.c_str());
    return descriptor ? avcodec_find_encoder(descriptor->id) : nullptr;
}

} // namespace

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planFileAudio(
    const std::string& inputPath,
    const MediaAudioPipelinePlannerOptions& options)
{
    MediaAudioPipelinePlan plan;
    if (!options.includeAudio) {
        plan.branchMode = MediaBranchMode::Drop;
        plan.reason = "disabled";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }

    auto probe = MediaPipelineAudioSourceProbe::probeFile(inputPath);
    if (!probe) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(probe.error());
    }
    if (!probe.value().found) {
        plan.branchMode = MediaBranchMode::Drop;
        plan.reason = "no_audio";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }

    const MediaPipelineAudioSourceProbeResult& source = probe.value();
    const bool canCopy = sourceMatchesRequestedTarget(source, options);
    const std::string sourceCodec = canonicalAudioCodecName(source.codecName);
    const std::string targetCodec = canonicalAudioCodecName(options.requestedCodecName.empty() ? sourceCodec : options.requestedCodecName);

    plan.enabled = true;
    plan.sourceStreamIndex = source.streamIndex;
    plan.sourceCodecName = sourceCodec;
    plan.targetCodecName = targetCodec;
    plan.branchMode = canCopy ? MediaBranchMode::CopyPacket : MediaBranchMode::TranscodeFrame;
    plan.followsSourceParameters = canCopy;
    plan.reason = canCopy ? "copy_source_matches_target" : "transcode_source_differs_from_target";

    if (plan.branchMode == MediaBranchMode::TranscodeFrame) {
        const AVCodec* encoder = findAudioEncoderForCodecName(targetCodec);
        if (!encoder || !encoder->name) {
            return ::media::Result<MediaAudioPipelinePlan>::failure(
                ::media::ErrorInfo::unsupported("audio encoder not found for codec: " + targetCodec));
        }
        plan.targetEncoderName = encoder->name;
    }

    return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
}

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planKnownAudioTranscode(
    MediaInputAudioStreamInfo inputInfo,
    const MediaAudioPipelinePlannerOptions& options)
{
    MediaAudioPipelinePlan plan;
    if (!options.includeAudio) {
        plan.branchMode = MediaBranchMode::Drop;
        plan.reason = "disabled";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }
    if (inputInfo.streamIndex < 0 || inputInfo.codecName.empty()) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planKnownAudioTranscode requires stream index and codec"));
    }

    const std::string sourceCodec = canonicalAudioCodecName(inputInfo.codecName);
    const std::string targetCodec = canonicalAudioCodecName(options.requestedCodecName.empty() ? sourceCodec : options.requestedCodecName);
    const AVCodec* encoder = findAudioEncoderForCodecName(targetCodec);
    if (!encoder || !encoder->name) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("audio encoder not found for codec: " + targetCodec));
    }

    plan.enabled = true;
    plan.branchMode = MediaBranchMode::TranscodeFrame;
    plan.sourceStreamIndex = inputInfo.streamIndex;
    plan.sourceCodecName = sourceCodec;
    plan.targetCodecName = targetCodec;
    plan.targetEncoderName = encoder->name;
    plan.followsSourceParameters = false;
    plan.reason = "realtime_transcode";
    return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
