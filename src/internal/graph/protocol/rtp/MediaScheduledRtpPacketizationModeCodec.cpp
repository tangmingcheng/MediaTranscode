#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationModeCodec.h"

namespace media::ffmpeg::graph {

std::string_view MediaScheduledRtpPacketizationModeCodec::encode(
    MediaScheduledRtpPacketizationMode mode) noexcept
{
    switch (mode) {
    case MediaScheduledRtpPacketizationMode::H264AnnexB:
        return "h264_annexb";
    case MediaScheduledRtpPacketizationMode::AacLatm:
        return "aac_latm";
    case MediaScheduledRtpPacketizationMode::HevcAnnexB:
        return "hevc_annexb";
    }
    return {};
}

::media::Result<MediaScheduledRtpPacketizationMode>
MediaScheduledRtpPacketizationModeCodec::decode(std::string_view value)
{
    for (const auto mode : {
             MediaScheduledRtpPacketizationMode::H264AnnexB,
             MediaScheduledRtpPacketizationMode::AacLatm,
             MediaScheduledRtpPacketizationMode::HevcAnnexB}) {
        if (value == encode(mode)) {
            return ::media::Result<MediaScheduledRtpPacketizationMode>::success(
                mode);
        }
    }
    return ::media::Result<MediaScheduledRtpPacketizationMode>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Unknown scheduled RTP packetization mode"));
}

} // namespace media::ffmpeg::graph
