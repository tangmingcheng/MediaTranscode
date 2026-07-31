#include "internal/graph/nodes/sync/MediaAvSyncSourceClockModeNodeOptionCodec.h"

namespace media::ffmpeg::graph {

::media::Result<std::string>
MediaAvSyncSourceClockModeNodeOptionCodec::encode(
    MediaAvSyncSourceClockMode mode)
{
    switch (mode) {
    case MediaAvSyncSourceClockMode::RtpSenderReports:
        return ::media::Result<std::string>::success(
            "rtp_sender_reports");
    case MediaAvSyncSourceClockMode::MpegTsPcr:
        return ::media::Result<std::string>::success("mpegts_pcr");
    case MediaAvSyncSourceClockMode::DemuxTimestamps:
        return ::media::Result<std::string>::success(
            "demux_timestamps");
    }
    return ::media::Result<std::string>::failure(
        ::media::ErrorInfo::unsupported(
            "A/V sync source clock mode cannot be encoded"));
}

::media::Result<MediaAvSyncSourceClockMode>
MediaAvSyncSourceClockModeNodeOptionCodec::decode(
    std::string_view text)
{
    if (text == "rtp_sender_reports") {
        return ::media::Result<MediaAvSyncSourceClockMode>::success(
            MediaAvSyncSourceClockMode::RtpSenderReports);
    }
    if (text == "mpegts_pcr") {
        return ::media::Result<MediaAvSyncSourceClockMode>::success(
            MediaAvSyncSourceClockMode::MpegTsPcr);
    }
    if (text == "demux_timestamps") {
        return ::media::Result<MediaAvSyncSourceClockMode>::success(
            MediaAvSyncSourceClockMode::DemuxTimestamps);
    }
    return ::media::Result<MediaAvSyncSourceClockMode>::failure(
        ::media::ErrorInfo::invalidArgument(
            "A/V sync source clock mode node option is invalid"));
}

} // namespace media::ffmpeg::graph
