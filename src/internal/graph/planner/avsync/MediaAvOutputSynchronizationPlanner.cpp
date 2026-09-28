#include "internal/graph/planner/avsync/MediaAvOutputSynchronizationPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvSynchronizationPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/planner/realtime/MediaMpegTsOutputTimingPlanner.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"

#include <cstdint>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

void planRtpOutput(MediaAvSyncPlan& plan,
                   const MediaAvOutputSynchronizationRequest& request,
                   int audioOutputRate)
{
    const std::string& groupIdentity = request.mediaId;
    const std::string cname =
        MediaRtpOutputIdentityPlanner::cname(groupIdentity);
    plan.rtpOutput.emplace();
    plan.rtpOutput->videoOutput.identity = groupIdentity + ".output.video";
    plan.rtpOutput->videoOutput.payloadType = 96;
    plan.rtpOutput->videoOutput.clockRate = 90'000;
    plan.rtpOutput->videoOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
            *plan.rtpOutput->videoOutput.identity);
    plan.rtpOutput->videoOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".video.timestamp");
    plan.rtpOutput->videoOutput.cname = cname;
    plan.rtpOutput->audioOutput.identity = groupIdentity + ".output.audio";
    plan.rtpOutput->audioOutput.payloadType = 97;
    plan.rtpOutput->audioOutput.clockRate = audioOutputRate;
    plan.rtpOutput->audioOutput.ssrc =
        MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
            *plan.rtpOutput->audioOutput.identity);
    plan.rtpOutput->audioOutput.baseTimestamp =
        MediaRtpOutputIdentityPlanner::stableNumeric(
            groupIdentity + ".audio.timestamp");
    plan.rtpOutput->audioOutput.cname = cname;
    plan.rtpOutput->output.useSharedNtpEpoch = true;
}

::media::Result<MediaTsMuxPlan> planTsOutput(
    const MediaAvSyncPlan& plan,
    const MediaAvOutputSynchronizationRequest& request,
    const MediaRealtimeDeploymentEnvelope& deploymentEnvelope,
    const MediaProjectMpegTsResolvedPipelineFacts& resolvedFacts)
{
    if (!plan.startup.outputLeadNs) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires planner-owned startup and transport timing"));
    }
    if (!request.transport) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS output requires an explicit transport"));
    }
    std::uint16_t maximumPacketsPerDatagram = 0;
    if (*request.transport == MediaOutputTransportKind::RtpAvp ||
        *request.transport == MediaOutputTransportKind::UdpDatagrams) {
        const auto maximumDatagram =
            deploymentEnvelope.encode().mtu.senderMaximumPayloadBytes;
        auto packetCount = MediaTsMuxPlan::maximumPacketsPerDatagram(
            static_cast<std::size_t>(maximumDatagram),
            *request.transport);
        if (!packetCount) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                packetCount.error());
        }
        maximumPacketsPerDatagram = packetCount.value();
    } else if (*request.transport !=
               MediaOutputTransportKind::UdpDatagrams) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "MPEG-TS output transport is unsupported"));
    }
    if (!request.videoFrameRate.complete() ||
        !request.videoFrameRate.numerator ||
        !request.videoFrameRate.denominator) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS transport timing requires prepared video cadence"));
    }
    auto audioCadence = MediaRunningTime::checkedFromTicks(
        resolvedFacts.audioOutput.codecFrameSamples(), 1,
        resolvedFacts.audioOutput.sampleRate());
    const auto& deployment = deploymentEnvelope.encode();
    auto timing = audioCadence
        ? MediaMpegTsOutputTimingPlanner::planVariableBitrate(
              deployment.latency.maximumReleaseJitter,
              deployment.latency.releaseJitterAuthority,
              MediaRational{
                  *request.videoFrameRate.numerator,
                  *request.videoFrameRate.denominator})
        : ::media::Result<MediaMpegTsTimingPolicy>::failure(
              audioCadence.error());
    auto preroll = timing
        ? MediaMpegTsOutputTimingPlanner::startupEmissionPreroll(
              deployment.transportTiming.senderTransportLead,
              MediaRational{
                  *request.videoFrameRate.numerator,
                  *request.videoFrameRate.denominator},
              audioCadence.value(), timing.value())
        : ::media::Result<MediaRunningTime>::failure(timing.error());
    if (!preroll) {
        return ::media::Result<MediaTsMuxPlan>::failure(preroll.error());
    }
    auto resolvedOutput = MediaProjectMpegTsOutputPlan::createAudioVideo(
        resolvedFacts.videoCodecName, resolvedFacts.videoPacketLayout,
        resolvedFacts.audioOutput, std::move(timing).value(),
        deployment.transportTiming.senderTransportLead,
        preroll.value(),
        *request.transport,
        maximumPacketsPerDatagram);
    if (!resolvedOutput) {
        return ::media::Result<MediaTsMuxPlan>::failure(resolvedOutput.error());
    }
    return ::media::Result<MediaTsMuxPlan>::success(
        resolvedOutput.value().muxPlan());
}

} // namespace

::media::Result<MediaAvSyncPlan> MediaAvOutputSynchronizationPlanner::plan(
    const MediaAvOutputSynchronizationRequest& request)
{
    if (request.mediaId.empty() || request.audioSampleRate <= 0) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Output synchronization requires media identity and resolved audio rate"));
    }
    MediaAvSyncPlan plan;
    plan.domainRole = MediaAvSyncDomainRole::ContinuousOutput;
    plan.controlGenerationPolicy = MediaControlGenerationPolicy::RequiredExact;
    plan.startup.requireVideoKeyFrame = true;
    plan.startup.outputLeadNs = request.deployment.encode().transportTiming.senderTransportLead;
    MediaAvSynchronizationPolicyPlanner::apply(plan);
    if (request.layout == RealtimeOutputStreamLayout::SeparateStreams) {
        if (request.transport != MediaOutputTransportKind::RtpAvp ||
            request.mpegTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Separate RTP output rejects MPEG-TS output facts"));
        }
        planRtpOutput(plan, request, request.audioSampleRate);
    } else if (request.layout == RealtimeOutputStreamLayout::MuxedTransportStream) {
        if (!request.mpegTsFacts) {
            return ::media::Result<MediaAvSyncPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Project MPEG-TS output requires resolved H.264/AAC pipeline facts"));
        }
        auto outputMux = planTsOutput(
            plan, request, request.deployment, *request.mpegTsFacts);
        if (!outputMux) {
            return ::media::Result<MediaAvSyncPlan>::failure(outputMux.error());
        }
        const bool useSharedNtpEpoch =
            outputMux.value().parameters().transportKind ==
            MediaOutputTransportKind::RtpAvp;
        plan.projectMpegTsOutput.emplace();
        plan.projectMpegTsOutput->outputMux = std::move(outputMux).value();
        plan.projectMpegTsOutput->useSharedNtpEpoch = useSharedNtpEpoch;
    } else {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Realtime A/V output adapter is not supported"));
    }

    if (auto status = MediaAvSyncPlanValidator::validateDomain(
            plan, MediaAvSyncDomainRole::ContinuousOutput); !status) {
        return ::media::Result<MediaAvSyncPlan>::failure(status.error());
    }
    return ::media::Result<MediaAvSyncPlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
