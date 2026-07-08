#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status positiveValue(const std::optional<int>& value, const char* name)
{
    if (value && *value <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(std::string(name) + " must be positive"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaAudioPipelinePlannerOptions> LocalFilePlannerRequestBuilder::buildAudioPlannerOptions(
    const LocalFileTranscodeOptions& options)
{
    const MediaTranscodeParameterSet& parameters = options.parameters;
    const MediaAudioTranscodeParameters& audio = parameters.audio;

    auto bitrate = positiveValue(audio.bitrateKbps, "audio bitrate");
    if (!bitrate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(bitrate.error());
    }
    auto minBitrate = positiveValue(audio.minBitrateKbps, "audio min bitrate");
    if (!minBitrate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(minBitrate.error());
    }
    auto maxBitrate = positiveValue(audio.maxBitrateKbps, "audio max bitrate");
    if (!maxBitrate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(maxBitrate.error());
    }
    auto bufferSize = positiveValue(audio.bufferSizeKbits, "audio buffer size");
    if (!bufferSize) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(bufferSize.error());
    }
    if (audio.minBitrateKbps && audio.maxBitrateKbps && *audio.minBitrateKbps > *audio.maxBitrateKbps) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument("audio min bitrate must be <= audio max bitrate"));
    }
    auto sampleRate = positiveValue(audio.sampleRate, "audio sample rate");
    if (!sampleRate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(sampleRate.error());
    }
    auto channels = positiveValue(audio.channels, "audio channels");
    if (!channels) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(channels.error());
    }
    auto quality = positiveValue(audio.quality, "audio quality");
    if (!quality) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(quality.error());
    }

    MediaAudioPipelinePlannerOptions plannerOptions(parameters.execution.includeAudio);
    plannerOptions.requestedCodecName = audio.codecName;
    plannerOptions.rateControl = audio.rateControl;
    plannerOptions.requestedBitrateKbps = audio.bitrateKbps;
    plannerOptions.requestedMinBitrateKbps = audio.minBitrateKbps;
    plannerOptions.requestedMaxBitrateKbps = audio.maxBitrateKbps;
    plannerOptions.requestedBufferSizeKbits = audio.bufferSizeKbits;
    plannerOptions.requestedSampleRate = audio.sampleRate;
    plannerOptions.requestedChannels = audio.channels;
    plannerOptions.requestedQuality = audio.quality;
    plannerOptions.requestedPreset = audio.preset;
    plannerOptions.requestedProfile = audio.profile;
    plannerOptions.diagnosticLogEnabled = parameters.execution.diagnosticLogEnabled;
    return ::media::Result<MediaAudioPipelinePlannerOptions>::success(std::move(plannerOptions));
}

} // namespace media::ffmpeg::graph
