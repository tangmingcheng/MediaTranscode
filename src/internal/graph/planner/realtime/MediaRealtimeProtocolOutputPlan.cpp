#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaProjectMpegTsRuntimeOutputPlan>
cloneMediaProjectMpegTsRuntimeOutputPlan(
    const MediaProjectMpegTsRuntimeOutputPlan& source)
{
    if (const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(&source.transport)) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::success(
            MediaProjectMpegTsRuntimeOutputPlan{
                source.protocol,
                source.muxSessionKind,
                source.emission,
                std::variant<MediaMpegTsUdpOutputPlan,
                             MediaMpegTsRtpOutputPlan>(
                    std::in_place_type<MediaMpegTsUdpOutputPlan>,
                    *udp)});
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(&source.transport);
    if (!rtp) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS runtime output has an unknown transport variant"));
    }
    auto clonedRtp = rtp->clone();
    if (!clonedRtp) {
        return ::media::Result<
            MediaProjectMpegTsRuntimeOutputPlan>::failure(
            clonedRtp.error());
    }
    return ::media::Result<
        MediaProjectMpegTsRuntimeOutputPlan>::success(
        MediaProjectMpegTsRuntimeOutputPlan{
            source.protocol,
            source.muxSessionKind,
            source.emission,
            std::variant<MediaMpegTsUdpOutputPlan,
                         MediaMpegTsRtpOutputPlan>(
                std::in_place_type<MediaMpegTsRtpOutputPlan>,
                std::move(clonedRtp).value())});
}

} // namespace media::ffmpeg::graph
