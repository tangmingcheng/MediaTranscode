#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

namespace media::ffmpeg::graph {

::media::Result<MediaScheduledRtpPacketizationPlan>
MediaScheduledRtpPacketizationPlan::create(
    MediaStreamKind streamKind, std::string codecName,
    int streamTimeBaseNumerator, int streamTimeBaseDenominator,
    int payloadType, std::size_t maximumDatagramBytes,
    std::optional<int> maximumAccessUnitSamples)
{
    codecName = canonicalCodecName(codecName);
    std::optional<MediaScheduledRtpPacketizationMode> mode;
    if (streamKind == MediaStreamKind::Video && codecName == "h264" &&
        !maximumAccessUnitSamples) {
        mode = MediaScheduledRtpPacketizationMode::H264AnnexB;
    } else if (streamKind == MediaStreamKind::Audio && codecName == "aac" &&
               maximumAccessUnitSamples && *maximumAccessUnitSamples > 0) {
        mode = MediaScheduledRtpPacketizationMode::AacLatm;
    }
    if (!mode) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "scheduled RTP packetization codec is unsupported: " +
                codecName));
    }
    if (streamTimeBaseNumerator <= 0 || streamTimeBaseDenominator <= 0 ||
        payloadType < 0 || payloadType > 127 || maximumDatagramBytes == 0) {
        return ::media::Result<MediaScheduledRtpPacketizationPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP packetization selection is incomplete"));
    }
    return ::media::Result<MediaScheduledRtpPacketizationPlan>::success(
        MediaScheduledRtpPacketizationPlan(
            streamKind, std::move(codecName), streamTimeBaseNumerator,
            streamTimeBaseDenominator, *mode, payloadType,
            maximumDatagramBytes, maximumAccessUnitSamples));
}

MediaScheduledRtpPacketizationPlan::MediaScheduledRtpPacketizationPlan(
    MediaStreamKind streamKind, std::string codecName,
    int streamTimeBaseNumerator, int streamTimeBaseDenominator,
    MediaScheduledRtpPacketizationMode packetizationMode, int payloadType,
    std::size_t maximumDatagramBytes,
    std::optional<int> maximumAccessUnitSamples)
    : m_streamKind(streamKind), m_codecName(std::move(codecName)),
      m_streamTimeBaseNumerator(streamTimeBaseNumerator),
      m_streamTimeBaseDenominator(streamTimeBaseDenominator),
      m_packetizationMode(packetizationMode), m_payloadType(payloadType),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_maximumAccessUnitSamples(maximumAccessUnitSamples)
{
}

} // namespace media::ffmpeg::graph
