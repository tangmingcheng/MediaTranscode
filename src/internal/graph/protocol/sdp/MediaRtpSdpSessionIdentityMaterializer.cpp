#include "internal/graph/protocol/sdp/MediaRtpSdpSessionIdentityMaterializer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaSdpSessionIdentity>
MediaRtpSdpSessionIdentityMaterializer::materialize(
    const MediaSeparateRtpSdpRuntimePlan& plan,
    const MediaSharedNtpEpoch& sharedNtpEpoch,
    std::uint64_t activePlaybackGeneration)
{
    if (plan.sessionIdPolicy != MediaRtpSdpSessionIdPolicy::SharedNtpEpoch ||
        plan.sessionVersionPolicy !=
            MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration ||
        activePlaybackGeneration == 0) {
        return ::media::Result<MediaSdpSessionIdentity>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP SDP session materialization policy is incomplete"));
    }
    auto capturedNtp = sharedNtpEpoch.map(
        sharedNtpEpoch.masterAtCapture());
    if (!capturedNtp) {
        return ::media::Result<MediaSdpSessionIdentity>::failure(
            capturedNtp.error());
    }
    const auto wire = capturedNtp.value().wire();
    const std::uint64_t sessionId =
        (static_cast<std::uint64_t>(wire.seconds) << 32) |
        static_cast<std::uint64_t>(wire.fraction);
    return MediaSdpSessionIdentity::create(
        plan.originUsername,
        sessionId,
        activePlaybackGeneration,
        plan.sessionName,
        plan.originAddressFamily,
        plan.originNumericAddress,
        plan.cname);
}

} // namespace media::ffmpeg::graph
