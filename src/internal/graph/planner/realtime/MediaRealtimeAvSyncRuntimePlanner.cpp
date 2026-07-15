#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <optional>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaScheduledRtpOutputPlan> scheduledRtpOutput(
    MediaScheduledStream stream,
    MediaRealtimeRtpOutputNodePlan& legacyOutput,
    const MediaAvSyncRtpOutputStreamPlan& synchronization,
    MediaRunningTime senderLead,
    MediaRunningTime senderReportInterval)
{
    if (!synchronization.payloadType || !synchronization.ssrc ||
        !synchronization.baseTimestamp || !synchronization.clockRate ||
        !synchronization.cname || synchronization.cname->empty() ||
        legacyOutput.packetSize <= 0 || !legacyOutput.scheduledTransport ||
        !legacyOutput.scheduledPacketization) {
        return ::media::Result<MediaScheduledRtpOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled RTP output requires complete protocol planning facts"));
    }
    return ::media::Result<MediaScheduledRtpOutputPlan>::success(
        MediaScheduledRtpOutputPlan{
            stream,
            std::move(*legacyOutput.scheduledTransport),
            *legacyOutput.scheduledPacketization,
            *synchronization.ssrc,
            *synchronization.baseTimestamp,
            *synchronization.clockRate,
            *synchronization.cname,
            senderLead,
            senderReportInterval});
}

} // namespace

::media::Result<MediaRealtimeAvSyncRuntimePlan>
MediaRealtimeAvSyncRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlan& outer,
    MediaAvSyncPlan synchronization)
{
    auto facts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, synchronization);
    if (!facts) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            facts.error());
    }
    auto correction = MediaAudioCorrectionReachabilityPlanner::plan(
        synchronization, facts.value());
    if (!correction || !facts.value().acknowledgementTimeout ||
        !facts.value().terminalDrainWindow || !synchronization.topology) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            correction ? ::media::ErrorInfo::notInitialized(
                             "A/V generation transition timing facts are incomplete")
                       : correction.error());
    }
    synchronization.audioServo.commandLeadNs = correction.value().commandLead;
    synchronization.audioServo.compensationWindowNs =
        correction.value().compensationWindow;
    synchronization.audioServo.frequencyFilterTimeConstantNs =
        correction.value().frequencyFilterTimeConstant;
    if (auto status = MediaAvSyncPlanValidator::validate(synchronization);
        !status) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            status.error());
    }

    MediaAvSyncOutputAdapterKind adapter;
    std::optional<std::variant<MediaSeparateRtpOutputRuntimePlan,
                               MediaProjectMpegTsRuntimeOutputPlan>> protocolOutput;
    if (*synchronization.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp) {
        if (!synchronization.rtp || synchronization.ts ||
            !synchronization.startup.outputLeadNs ||
            !synchronization.rtp->output.senderReportIntervalNs ||
            outer.sdp.path.empty() || !outer.audioPlan.resolvedOutput) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP synchronization output facts are incomplete"));
        }
        auto video = scheduledRtpOutput(
            MediaScheduledStream::Video,
            outer.videoOutput,
            synchronization.rtp->videoOutput,
            *synchronization.startup.outputLeadNs,
            *synchronization.rtp->output.senderReportIntervalNs);
        auto audio = scheduledRtpOutput(
            MediaScheduledStream::Audio,
            outer.audioOutput,
            synchronization.rtp->audioOutput,
            *synchronization.startup.outputLeadNs,
            *synchronization.rtp->output.senderReportIntervalNs);
        if (!video || !audio) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                video ? audio.error() : video.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp;
        protocolOutput.emplace(std::in_place_type<MediaSeparateRtpOutputRuntimePlan>,
            MediaSeparateRtpOutputRuntimePlan{
                std::move(video).value(),
                std::move(audio).value(),
                outer.sdp.path});
    } else if (*synchronization.topology ==
               MediaAvSyncTopology::MpegTsToMpegTs) {
        if (synchronization.rtp || !synchronization.ts ||
            !synchronization.ts->outputMux ||
            !facts.value().outputSampleRate || outer.muxedOutput.url.empty()) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS synchronization output facts are incomplete"));
        }
        auto accepted = MediaProjectMpegTsOutputPlan::accept(
            *facts.value().outputSampleRate,
            *synchronization.ts->outputMux);
        if (!accepted) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                accepted.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
        outer.muxedOutput.outputResourceKind = MediaOutputResourceKind::ByteSink;
        outer.muxedOutput.muxSessionKind = MediaMuxSessionKind::ProjectMpegTs;
        protocolOutput.emplace(std::in_place_type<MediaProjectMpegTsRuntimeOutputPlan>,
            MediaProjectMpegTsRuntimeOutputPlan{
                outer.muxedOutput.url,
                MediaOutputResourceKind::ByteSink,
                MediaMuxSessionKind::ProjectMpegTs,
                std::move(accepted).value()});
    } else {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V runtime plan topology is unsupported"));
    }

    auto transition = MediaAvGenerationTransitionPlanner::plan(
        adapter,
        *facts.value().acknowledgementTimeout,
        *facts.value().terminalDrainWindow);
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
            MediaAvSyncGroupKey("realtime.av"),
            std::move(synchronization),
            adapter,
            std::move(*protocolOutput),
            outer.queues,
            outer.edgePolicies,
            outer.threadingPolicy,
            std::move(transition),
            facts.value(),
            correction.value().correction});
}

} // namespace media::ffmpeg::graph
