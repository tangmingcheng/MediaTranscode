#include "internal/graph/protocol/rtp/MediaRtcpWireDatagramComposer.h"

#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"

#include <string>

namespace media::ffmpeg::graph {

::media::Result<std::vector<std::uint8_t>>
MediaRtcpWireDatagramComposer::composeSenderReport(
    std::uint32_t ssrc,
    std::string_view cname,
    const MediaRtcpSenderReportScheduleDecision& decision,
    const MediaSharedNtpEpoch& ntpEpoch,
    const MediaRtpOutputClockMapper& clockMapper,
    std::uint64_t packetCount,
    std::uint64_t octetCount)
{
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        decision.reportInstant, ntpEpoch, clockMapper);
    if (!timestamp) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            timestamp.error());
    }
    return MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            ssrc,
            std::string(cname),
            timestamp.value(),
            packetCount,
            octetCount));
}

::media::Result<std::vector<std::uint8_t>>
MediaRtcpWireDatagramComposer::composeTerminalReport(
    std::uint32_t ssrc,
    std::string_view cname,
    MediaRunningTime reportInstant,
    const MediaSharedNtpEpoch& ntpEpoch,
    const MediaRtpOutputClockMapper& clockMapper,
    std::uint64_t packetCount,
    std::uint64_t octetCount)
{
    auto timestamp = MediaRtcpSenderReportGenerator::mapTimestamp(
        reportInstant, ntpEpoch, clockMapper);
    if (!timestamp) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            timestamp.error());
    }
    return MediaRtcpSenderReportGenerator::serializeWithBye(
        MediaRtcpSenderReportParameters(
            ssrc,
            std::string(cname),
            timestamp.value(),
            packetCount,
            octetCount));
}

} // namespace media::ffmpeg::graph
