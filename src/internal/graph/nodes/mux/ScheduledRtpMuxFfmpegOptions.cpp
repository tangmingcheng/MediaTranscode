#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegOptions.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

#include <cstdint>

namespace media::ffmpeg::graph {
namespace {

const char* plannedFlags(const ScheduledRtpMuxStreamConfig& config) noexcept
{
    return config.packetizationMode() ==
                   MediaScheduledRtpPacketizationMode::AacLatm
        ? "skip_rtcp+latm"
        : "skip_rtcp";
}

} // namespace

::media::Status ScheduledRtpMuxFfmpegOptions::apply(
    void* muxerPrivateData,
    const ScheduledRtpMuxStreamConfig& config)
{
    if (!muxerPrivateData) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP mux private options are unavailable"));
    }
    int result = av_opt_set_int(
        muxerPrivateData, "payload_type", config.identity().payloadType(), 0);
    if (result >= 0) {
        result = av_opt_set_int(
            muxerPrivateData, "ssrc", config.identity().ssrc(), 0);
    }
    if (result >= 0) {
        result = av_opt_set(
            muxerPrivateData, "rtpflags", plannedFlags(config), 0);
    }
    return result < 0
        ? FFmpegGraphError::statusFromCode(
              result, "av_opt_set(scheduled rtp authority options)")
        : ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegOptions::verify(
    void* muxerPrivateData,
    const ScheduledRtpMuxStreamConfig& config)
{
    if (!muxerPrivateData) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP mux options cannot be verified"));
    }
    std::int64_t payloadType = -1;
    std::int64_t ssrc = -1;
    std::int64_t rtpFlags = 0;
    const int payloadResult = av_opt_get_int(
        muxerPrivateData, "payload_type", 0, &payloadType);
    const int ssrcResult = av_opt_get_int(
        muxerPrivateData, "ssrc", 0, &ssrc);
    const int flagsResult = av_opt_get_int(
        muxerPrivateData, "rtpflags", 0, &rtpFlags);
    if (payloadResult < 0 || ssrcResult < 0 || flagsResult < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP mux did not expose configured authority options"));
    }
    const AVOption* flagsOption = av_opt_find(
        muxerPrivateData, "rtpflags", nullptr, 0, 0);
    int expectedFlags = 0;
    const int evaluated = flagsOption
        ? av_opt_eval_flags(muxerPrivateData,
                            flagsOption,
                            plannedFlags(config),
                            &expectedFlags)
        : AVERROR_OPTION_NOT_FOUND;
    if (payloadType != config.identity().payloadType() ||
        static_cast<std::uint32_t>(ssrc) != config.identity().ssrc() ||
        evaluated < 0 || rtpFlags != expectedFlags) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP mux authority options differ from the complete plan"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
