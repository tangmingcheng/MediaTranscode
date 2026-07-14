#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include "internal/graph/planner/MediaPipelineAudioSourceProbe.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::optional<MediaAudioProfile>> requestedProfile(
    const std::string& codecName,
    const std::string& profileName)
{
    if (profileName.empty()) {
        return ::media::Result<std::optional<MediaAudioProfile>>::success(std::nullopt);
    }
    auto profile = MediaAudioProfile::fromCodecProfile(codecName, profileName);
    if (!profile) return ::media::Result<std::optional<MediaAudioProfile>>::failure(profile.error());
    return ::media::Result<std::optional<MediaAudioProfile>>::success(profile.value());
}

::media::Result<MediaSelectedAudioEncoder> selectAudioEncoder(const std::string& codecName)
{
    const AVCodec* encoder = avcodec_find_encoder_by_name(codecName.c_str());
    if (!encoder) {
        const AVCodecDescriptor* descriptor = avcodec_descriptor_get_by_name(codecName.c_str());
        encoder = descriptor ? avcodec_find_encoder(descriptor->id) : nullptr;
    }
    if (!encoder || !encoder->name || !encoder->sample_fmts ||
        encoder->sample_fmts[0] == AV_SAMPLE_FMT_NONE) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported("complete audio encoder capability not found for codec: " + codecName));
    }
    const char* sampleFormat = av_get_sample_fmt_name(encoder->sample_fmts[0]);
    if (!sampleFormat) {
        return ::media::Result<MediaSelectedAudioEncoder>::failure(
            ::media::ErrorInfo::unsupported("audio encoder sample format is unknown: " + codecName));
    }
    return ::media::Result<MediaSelectedAudioEncoder>::success({encoder->name, sampleFormat});
}

MediaResolvedAudioSource resolvedSource(const MediaInputAudioStreamInfo& input)
{
    return MediaResolvedAudioSource{
        canonicalCodecName(input.codecName), input.profile, input.sampleRate, input.channels,
        input.channelLayout, input.sampleFormat, input.bitrateBitsPerSecond};
}

::media::Result<MediaResolvedAudioRequest> resolvedRequest(
    const MediaAudioPipelinePlannerOptions& options,
    const MediaResolvedAudioSource& source)
{
    MediaResolvedAudioRequest request;
    request.codecName = options.requestedCodecName;
    const std::string targetCodec = canonicalCodecName(
        request.codecName.empty() ? source.codecName : request.codecName);
    auto profile = requestedProfile(targetCodec, options.requestedProfile);
    if (!profile) return ::media::Result<MediaResolvedAudioRequest>::failure(profile.error());
    request.profile = profile.value();
    const bool encoderOnlyRequest = options.rateControl != MediaRateControlMode::Auto ||
        options.requestedBitrateKbps || options.requestedMinBitrateKbps ||
        options.requestedMaxBitrateKbps || options.requestedBufferSizeKbits ||
        options.requestedQuality || !options.requestedPreset.empty();
    const bool formatChange = options.requestedSampleRate || options.requestedChannels;
    if (!request.profile && targetCodec == "aac" &&
        (targetCodec != source.codecName || formatChange || encoderOnlyRequest)) {
        request.profile = MediaAudioProfile::knownAacLow();
    }
    request.sampleRate = options.requestedSampleRate;
    request.channels = options.requestedChannels;
    request.rateControl = options.rateControl;
    request.bitrateKbps = options.requestedBitrateKbps;
    request.minBitrateKbps = options.requestedMinBitrateKbps;
    request.maxBitrateKbps = options.requestedMaxBitrateKbps;
    request.bufferSizeKbits = options.requestedBufferSizeKbits;
    request.quality = options.requestedQuality;
    request.preset = options.requestedPreset;
    return ::media::Result<MediaResolvedAudioRequest>::success(std::move(request));
}

} // namespace

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planFileAudio(
    const std::string& inputPath,
    const MediaAudioPipelinePlannerOptions& options)
{
    if (!options.includeAudio) {
        MediaAudioPipelinePlan plan;
        plan.branchMode = MediaBranchMode::Drop;
        plan.reason = "disabled";
        return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
    }
    auto probe = MediaPipelineAudioSourceProbe::probeFile(inputPath);
    if (!probe) return ::media::Result<MediaAudioPipelinePlan>::failure(probe.error());
    if (!probe.value().found) {
        return ::media::Result<MediaAudioPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("audio is enabled but input audio stream was not found; pass --no-audio to disable audio"));
    }
    const auto profile = MediaAudioProfile::fromCodecProfile(
        probe.value().descriptor.codec.codecName, probe.value().descriptor.codec.profile);
    if (!profile) return ::media::Result<MediaAudioPipelinePlan>::failure(profile.error());
    MediaInputAudioStreamInfo input;
    input.streamIndex = probe.value().streamIndex;
    input.codecName = probe.value().descriptor.codec.codecName;
    input.profile = profile.value();
    input.sampleRate = probe.value().descriptor.audio.sampleRate;
    input.channels = probe.value().descriptor.audio.channels;
    input.channelLayout = probe.value().descriptor.audio.channelLayout;
    input.sampleFormat = probe.value().descriptor.audio.sampleFormat;
    input.bitrateBitsPerSecond = probe.value().descriptor.codec.bitrate;
    return planKnownAudio(std::move(input), options);
}

::media::Result<MediaAudioPipelinePlan> MediaAudioPipelinePlanner::planKnownAudio(
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
            ::media::ErrorInfo::invalidArgument("planKnownAudio requires stream index and codec"));
    }
    const MediaResolvedAudioSource source = resolvedSource(inputInfo);
    auto request = resolvedRequest(options, source);
    if (!request) return ::media::Result<MediaAudioPipelinePlan>::failure(request.error());
    const std::string targetCodec = canonicalCodecName(request.value().codecName.empty()
        ? (options.outputRequirement.codecName ? *options.outputRequirement.codecName : source.codecName)
        : request.value().codecName);
    auto encoder = selectAudioEncoder(targetCodec);
    if (!encoder) return ::media::Result<MediaAudioPipelinePlan>::failure(encoder.error());
    auto output = MediaResolvedAudioOutputPlan::create(
        source, request.value(), options.outputRequirement, encoder.value());
    if (!output) return ::media::Result<MediaAudioPipelinePlan>::failure(output.error());

    plan.enabled = true;
    plan.sourceStreamIndex = inputInfo.streamIndex;
    plan.sourceCodecName = source.codecName;
    plan.branchMode = output.value().branchMode();
    plan.monotonicPacketTimestamps = plan.branchMode == MediaBranchMode::CopyPacket;
    plan.reason = plan.branchMode == MediaBranchMode::CopyPacket
        ? "copy_source_matches_resolved_output" : "transcode_source_differs_from_resolved_output";
    plan.resolvedOutput = std::move(output).value();
    return ::media::Result<MediaAudioPipelinePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
