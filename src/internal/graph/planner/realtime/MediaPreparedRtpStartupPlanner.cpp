#include "internal/graph/planner/realtime/MediaPreparedRtpStartupPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedInputRetentionPlan>
MediaPreparedRtpStartupPlanner::plan(
    const MediaRealtimeRawInputPlan& input,
    const MediaPreparedRealtimeInput& video,
    const MediaPreparedRealtimeInput* audio,
    MediaRunningTime acquisitionWindow)
{
    using Result = ::media::Result<MediaPreparedInputRetentionPlan>;
    const bool hasAudio = input.audio.has_value();
    if (hasAudio != (audio != nullptr) ||
        hasAudio != input.audioDepacketizer.has_value() ||
        hasAudio != input.audioAccessUnitEnvelope.has_value() ||
        (hasAudio && (input.audioDepacketizer->clockRate <= 0 ||
                     input.audioDepacketizer->accessUnitDurationRtpTicks <= 0))) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RTP startup retention requires consistent prepared source members and AU duration"));
    }
    auto videoReplay = video.rawRtpReplayAccessUnitBound();
    if (!videoReplay) return Result::failure(videoReplay.error());
    const auto& videoAu = input.videoAccessUnitEnvelope;
    auto videoRetention = MediaPreparedInputRetentionPlanner::planStream(
        acquisitionWindow, input.video.frameRate, videoReplay.value(),
        {MediaStreamKind::Video, videoAu.maximumAccessUnitBytes, videoAu.sizeAuthority},
        "prepared-source-RTP-marker-cadence-finite-admission");
    if (!videoRetention) return Result::failure(videoRetention.error());
    MediaPreparedInputRetentionPlan result{
        acquisitionWindow, std::move(videoRetention).value(), std::nullopt};
    std::string audioDiagnostic;
    if (audio) {
        auto audioReplay = audio->rawRtpReplayAccessUnitBound();
        if (!audioReplay) return Result::failure(audioReplay.error());
        const auto& audioAu = *input.audioAccessUnitEnvelope;
        auto audioRetention = MediaPreparedInputRetentionPlanner::planStream(
            acquisitionWindow,
            {input.audioDepacketizer->clockRate,
             input.audioDepacketizer->accessUnitDurationRtpTicks}, audioReplay.value(),
            {MediaStreamKind::Audio, audioAu.maximumAccessUnitBytes, audioAu.sizeAuthority},
            "source-audio-clock-rate-and-AU-duration-finite-admission");
        if (!audioRetention) return Result::failure(audioRetention.error());
        result.audio = std::move(audioRetention).value();
        audioDiagnostic = " audio_replay_au_bound=" + std::to_string(audioReplay.value()) +
            " audio_units=" + std::to_string(result.audio->maximumUnits) +
            " audio_bytes=" + std::to_string(result.audio->maximumBytes);
    }
    if (auto status = result.validate(); !status) return Result::failure(status.error());
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::PlannerSelect,
        "input_startup_retention window_ns=" + std::to_string(acquisitionWindow.nanoseconds()) +
        " video_replay_au_bound=" + std::to_string(videoReplay.value()) +
        " video_units=" + std::to_string(result.video.maximumUnits) +
        " video_bytes=" + std::to_string(result.video.maximumBytes) +
        audioDiagnostic +
        " future_arrival_rate_guarantee=not_proven");
    return Result::success(std::move(result));
}

} // namespace media::ffmpeg::graph
