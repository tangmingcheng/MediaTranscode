#include "internal/graph/planner/realtime/MediaPreparedRtpStartupPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedInputRetentionPlan>
MediaPreparedRtpStartupPlanner::plan(
    const MediaRealtimeRawInputPlan& input,
    const MediaPreparedRealtimeInput& video,
    const MediaPreparedRealtimeInput& audio,
    MediaRunningTime acquisitionWindow)
{
    using Result = ::media::Result<MediaPreparedInputRetentionPlan>;
    if (!input.audioDepacketizer || !input.audioAccessUnitEnvelope ||
        input.audioDepacketizer->clockRate <= 0 ||
        input.audioDepacketizer->accessUnitDurationRtpTicks <= 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RTP startup retention requires prepared source audio AU duration"));
    }
    auto videoReplay = video.rawRtpReplayAccessUnitBound();
    auto audioReplay = audio.rawRtpReplayAccessUnitBound();
    if (!videoReplay || !audioReplay) return Result::failure(
        !videoReplay ? videoReplay.error() : audioReplay.error());
    const auto& videoAu = input.videoAccessUnitEnvelope;
    const auto& audioAu = *input.audioAccessUnitEnvelope;
    auto videoRetention = MediaPreparedInputRetentionPlanner::planStream(
        acquisitionWindow, input.video.frameRate, videoReplay.value(),
        {MediaStreamKind::Video, videoAu.maximumAccessUnitBytes, videoAu.sizeAuthority},
        "prepared-source-RTP-marker-cadence-finite-admission");
    auto audioRetention = MediaPreparedInputRetentionPlanner::planStream(
        acquisitionWindow,
        {input.audioDepacketizer->clockRate,
         input.audioDepacketizer->accessUnitDurationRtpTicks}, audioReplay.value(),
        {MediaStreamKind::Audio, audioAu.maximumAccessUnitBytes, audioAu.sizeAuthority},
        "source-audio-clock-rate-and-AU-duration-finite-admission");
    if (!videoRetention || !audioRetention) return Result::failure(
        !videoRetention ? videoRetention.error() : audioRetention.error());
    MediaPreparedInputRetentionPlan result{
        acquisitionWindow, std::move(videoRetention).value(), std::move(audioRetention).value()};
    if (auto status = result.validate(); !status) return Result::failure(status.error());
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::PlannerSelect,
        "input_startup_retention window_ns=" + std::to_string(acquisitionWindow.nanoseconds()) +
        " video_replay_au_bound=" + std::to_string(videoReplay.value()) +
        " audio_replay_au_bound=" + std::to_string(audioReplay.value()) +
        " video_units=" + std::to_string(result.video.maximumUnits) +
        " audio_units=" + std::to_string(result.audio.maximumUnits) +
        " video_bytes=" + std::to_string(result.video.maximumBytes) +
        " audio_bytes=" + std::to_string(result.audio.maximumBytes) +
        " future_arrival_rate_guarantee=not_proven");
    return Result::success(std::move(result));
}

} // namespace media::ffmpeg::graph
