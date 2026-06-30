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
    auto bitrate = positiveValue(options.audioBitrateKbps, "audio bitrate");
    if (!bitrate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(bitrate.error());
    }
    auto sampleRate = positiveValue(options.audioSampleRate, "audio sample rate");
    if (!sampleRate) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(sampleRate.error());
    }
    auto channels = positiveValue(options.audioChannels, "audio channels");
    if (!channels) {
        return ::media::Result<MediaAudioPipelinePlannerOptions>::failure(channels.error());
    }

    MediaAudioPipelinePlannerOptions plannerOptions;
    plannerOptions.includeAudio = options.includeAudio;
    plannerOptions.transformRequested = options.audioTranscode;
    plannerOptions.requestedCodecName = options.audioCodec;
    plannerOptions.requestedBitrateKbps = options.audioBitrateKbps;
    plannerOptions.requestedSampleRate = options.audioSampleRate;
    plannerOptions.requestedChannels = options.audioChannels;
    plannerOptions.diagnosticLogEnabled = options.diagnosticLogEnabled;
    return ::media::Result<MediaAudioPipelinePlannerOptions>::success(std::move(plannerOptions));
}

} // namespace media::ffmpeg::graph
