#include "internal/graph/planner/realtime/MediaRealtimeRtpVideoSignalingResolver.h"

#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::string>
MediaRealtimeRtpVideoSignalingResolver::resolveFmtp(
    const MediaRealtimeRtpInputMetadata& requested,
    const MediaDetectedRtpVideoSignaling* detected)
{
    const std::string codecName = canonicalCodecName(requested.codecName);
    MediaRtpVideoSignalingFacts facts;
    if (detected) {
        if (requested.fmtp || !requested.payloadType ||
            !requested.clockRate) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "detected raw RTP video signaling requires explicit request identity without manual fmtp"));
        }
        if (detected->codecName != codecName ||
            detected->payloadType != *requested.payloadType ||
            detected->clockRate != *requested.clockRate) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP detected signaling identity conflicts with request"));
        }
        facts = detected->facts;
    } else {
        if (!requested.fmtp) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP video signaling requires manual fmtp or detected facts"));
        }
        auto parsed = parseRtpVideoSignalingFacts(
            codecName, *requested.fmtp);
        if (!parsed) {
            return ::media::Result<std::string>::failure(parsed.error());
        }
        facts = std::move(parsed).value();
    }

    if (auto status = MediaRtpVideoParameterSetValidator::validate(
            codecName, facts);
        !status) {
        return ::media::Result<std::string>::failure(status.error());
    }
    return serializeRtpVideoFmtp(facts);
}

} // namespace media::ffmpeg::graph
