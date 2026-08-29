#include "internal/graph/planner/MediaTranscodeStreamSetRequestValidator.h"

namespace media::ffmpeg::graph {
namespace {

bool audioControlSpecified(const MediaAudioTranscodeParameters& audio) noexcept
{
    return !audio.codecName.empty() ||
        audio.rateControl != MediaRateControlMode::Auto ||
        audio.bitrateKbps.has_value() ||
        audio.minBitrateKbps.has_value() ||
        audio.maxBitrateKbps.has_value() ||
        audio.sampleRate.has_value() ||
        audio.channels.has_value() ||
        audio.quality.has_value() ||
        !audio.preset.empty() ||
        !audio.profile.empty();
}

} // namespace

::media::Status MediaTranscodeStreamSetRequestValidator::validate(
    const MediaTranscodeParameterSet& parameters)
{
    if (!parameters.execution.streamSet) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Transcode stream set must be explicit"));
    }

    switch (*parameters.execution.streamSet) {
    case MediaTranscodeStreamSet::AudioVideo:
        return ::media::Status::success();
    case MediaTranscodeStreamSet::VideoOnly:
        if (audioControlSpecified(parameters.audio)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "VideoOnly rejects explicit audio transcode controls"));
        }
        return ::media::Status::success();
    }

    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "Transcode stream set is not supported"));
}

} // namespace media::ffmpeg::graph
