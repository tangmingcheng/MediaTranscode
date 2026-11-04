#include "internal/graph/protocol/rtp/MediaRtcpWireDatagramComposer.h"

namespace media::ffmpeg::graph {
::media::Result<MediaRtcpSenderReportPlan> MediaRtcpWireDatagramComposer::prepare(
    std::uint32_t ssrc, std::string_view cname, MediaRunningTime reportInstant,
    const MediaSharedNtpEpoch& ntpEpoch, const MediaRtpOutputClockMapper& clockMapper,
    std::uint64_t packetCount, std::uint64_t octetCount, bool includeBye)
{
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        reportInstant, ntpEpoch, clockMapper);
    if (!timestamp) return ::media::Result<MediaRtcpSenderReportPlan>::failure(timestamp.error());
    return MediaRtcpSenderReportGenerator::prepare(
        MediaRtcpSenderReportParameters(ssrc, cname, timestamp.value(), packetCount, octetCount),
        includeBye);
}
} // namespace media::ffmpeg::graph
