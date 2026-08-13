#include "internal/graph/planner/realtime/MediaRealtimeAudioPlannerOptionsResolver.h"

#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status validatePositiveOptional(
    const std::optional<int>& value,
    const char* name)
{
    if (value && *value <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(name) + " must be positive"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaAudioPipelinePlannerOptions>
MediaRealtimeAudioPlannerOptionsResolver::resolve(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.parameters.execution.streamSet) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime audio planner options require a validated stream set"));
    }

    const MediaAudioTranscodeParameters& audio = request.parameters.audio;
    const struct Validation final {
        const std::optional<int>& value;
        const char* name;
    } validations[] = {
        {audio.bitrateKbps, "audio bitrate"},
        {audio.minBitrateKbps, "audio min bitrate"},
        {audio.maxBitrateKbps, "audio max bitrate"},
        {audio.bufferSizeKbits, "audio buffer size"},
        {audio.sampleRate, "audio sample rate"},
        {audio.channels, "audio channels"},
        {audio.quality, "audio quality"},
    };
    for (const auto& validation : validations) {
        if (auto status = validatePositiveOptional(
                validation.value, validation.name); !status) {
            return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(
                status.error());
        }
    }
    if (audio.minBitrateKbps && audio.maxBitrateKbps &&
        *audio.minBitrateKbps > *audio.maxBitrateKbps) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio min bitrate must be <= audio max bitrate"));
    }

    MediaAudioPipelinePlannerOptions options(
        *request.parameters.execution.streamSet);
    options.requestedCodecName = audio.codecName;
    options.rateControl = audio.rateControl;
    options.requestedBitrateKbps = audio.bitrateKbps;
    options.requestedMinBitrateKbps = audio.minBitrateKbps;
    options.requestedMaxBitrateKbps = audio.maxBitrateKbps;
    options.requestedBufferSizeKbits = audio.bufferSizeKbits;
    options.requestedSampleRate = audio.sampleRate;
    options.requestedChannels = audio.channels;
    options.requestedQuality = audio.quality;
    options.requestedPreset = audio.preset;
    options.requestedProfile = audio.profile;
    if (MediaRealtimeRequestClassifier::muxedTransportOutput(request)) {
        options.outputRequirement.codecName = "aac";
        options.outputRequirement.profile = MediaAudioProfile::knownAacLow();
        options.outputRequirement.sampleRate = 48'000;
        options.outputRequirement.channels = 2;
    }
    options.diagnosticLogEnabled =
        request.parameters.execution.diagnosticLogEnabled;
    return ::media::Result<MediaAudioPipelinePlannerOptions>::success(
        std::move(options));
}

} // namespace media::ffmpeg::graph
