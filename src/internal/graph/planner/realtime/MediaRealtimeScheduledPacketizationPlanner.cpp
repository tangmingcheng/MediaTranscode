#include "internal/graph/planner/realtime/MediaRealtimeScheduledPacketizationPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpEmissionPlanner.h"
namespace media::ffmpeg::graph {
::media::Result<MediaScheduledRtpPacketizationPlan>
MediaRealtimeScheduledPacketizationPlanner::planVideo(
    const MediaVideoOutputPlan& plan,
    const MediaRealtimeScheduledRtpOutputPlanningDraft& output,
    int clockRate,
    int payloadType)
{
    if (output.packetSize <= 0 || clockRate <= 0 || payloadType < 0 ||
        payloadType > 127) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled video RTP packetization requires complete protocol facts"));
    }
    if (!plan.encoder.encodedPacketLayout ||
        plan.encoder.encodedPacketLayout->kind() != MediaEncodedPacketLayoutKind::StartCodeDelimited)
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::notInitialized("scheduled RTP requires start-code encoder packet layout"));
    const auto& prepared = plan.encoder.preparedEmission;
    if (!prepared || !plan.encoder.preparedCodecParameters ||
        prepared->encodedPacketLayout != plan.encoder.encodedPacketLayout) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled video RTP requires consistent prepared encoder facts"));
    }
    auto emission = MediaPreparedRtpEmissionPlanner::video(
        *prepared, *plan.encoder.preparedCodecParameters,
        static_cast<std::size_t>(output.packetSize));
    if (!emission) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            emission.error());
    }
    return MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, plan.encoder.codecName, 1,
        clockRate, payloadType,
        static_cast<std::size_t>(output.packetSize),
        std::move(emission).value());
}

::media::Status MediaRealtimeScheduledPacketizationPlanner::planAudioVideo(
    const MediaVideoOutputPlan& plan,
    const MediaAudioPipelinePlan& audio,
    MediaRealtimeOutputPlanningDraft& output,
    const MediaAvSyncPlan& synchronization)
{
    if (!synchronization.rtpOutput ||
        !synchronization.rtpOutput->videoOutput.clockRate ||
        !synchronization.rtpOutput->videoOutput.payloadType ||
        !synchronization.rtpOutput->audioOutput.clockRate ||
        !synchronization.rtpOutput->audioOutput.payloadType ||
        !audio.resolvedOutput || output.audioOutput.packetSize <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "scheduled RTP packetization requires complete selected protocol facts"));
    }
    auto video = MediaRealtimeScheduledPacketizationPlanner::planVideo(
        plan, output.videoOutput,
        *synchronization.rtpOutput->videoOutput.clockRate,
        *synchronization.rtpOutput->videoOutput.payloadType);
    if (!audio.preparedEmission) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "scheduled audio RTP requires prepared encoder emission"));
    }
    auto audioEmission = MediaPreparedRtpEmissionPlanner::audio(
        *audio.preparedEmission,
        static_cast<std::size_t>(output.audioOutput.packetSize));
    if (!audioEmission) {
        return ::media::Status::failure(audioEmission.error());
    }
    auto audioPacketization = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, audio.resolvedOutput->codecName(), 1,
        *synchronization.rtpOutput->audioOutput.clockRate,
        *synchronization.rtpOutput->audioOutput.payloadType,
        static_cast<std::size_t>(output.audioOutput.packetSize),
        std::move(audioEmission).value(),
        audio.resolvedOutput->codecFrameSamples());
    if (!video || !audioPacketization) {
        return ::media::Status::failure(
            video ? audioPacketization.error() : video.error());
    }
    output.videoOutput.scheduledPacketization = std::move(video).value();
    output.audioOutput.scheduledPacketization =
        std::move(audioPacketization).value();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
