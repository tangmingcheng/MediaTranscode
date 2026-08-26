#include "internal/graph/protocol/rtp/MediaRtpAccessUnitEmissionContract.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"
#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t RtpHeaderBytes = 12;

::media::Result<std::uint64_t> aacLatmDatagrams(
    std::uint64_t maximumAccessUnitBytes,
    std::uint64_t maximumRtpPayloadBytes)
{
    auto lengthBytes = MediaRealtimePlanningArithmetic::add(
        maximumAccessUnitBytes / 255U, 1U,
        "AAC LATM PayloadLengthInfo bytes");
    if (!lengthBytes || lengthBytes.value() >= maximumRtpPayloadBytes) {
        return ::media::Result<std::uint64_t>::failure(
            lengthBytes ? ::media::ErrorInfo::unsupported(
                "AAC LATM length field cannot fit the planned RTP payload")
                        : lengthBytes.error());
    }
    const auto firstPayload = maximumRtpPayloadBytes - lengthBytes.value();
    const auto remainder = maximumAccessUnitBytes > firstPayload
        ? maximumAccessUnitBytes - firstPayload : 0U;
    auto trailing = MediaRealtimePlanningArithmetic::ceilScale(
        remainder, 1U, maximumRtpPayloadBytes,
        "AAC LATM trailing RTP datagrams");
    return trailing
        ? MediaRealtimePlanningArithmetic::add(
              trailing.value(), 1U, "AAC LATM first RTP datagram")
        : trailing;
}

} // namespace

MediaRtpAccessUnitEmissionContract::MediaRtpAccessUnitEmissionContract(
    std::uint64_t maximumAccessUnitPayloadBytes,
    std::uint64_t maximumDatagramsPerAccessUnit,
    MediaScheduledRtpPacketizationMode packetizationMode,
    std::size_t maximumDatagramBytes,
    std::optional<MediaEncodedPacketLayout> packetLayout,
    std::string authority) noexcept
    : m_maximumAccessUnitPayloadBytes(maximumAccessUnitPayloadBytes),
      m_maximumDatagramsPerAccessUnit(maximumDatagramsPerAccessUnit),
      m_packetizationMode(packetizationMode),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_packetLayout(std::move(packetLayout)),
      m_authority(std::move(authority))
{
}

::media::Result<MediaRtpAccessUnitEmissionContract>
MediaRtpAccessUnitEmissionContract::createVideo(
    MediaScheduledRtpPacketizationMode mode,
    MediaEncodedPacketLayout layout,
    std::uint64_t maximumAccessUnitPayloadBytes,
    std::size_t maximumDatagramBytes,
    std::string authority)
{
    using Result = ::media::Result<MediaRtpAccessUnitEmissionContract>;
    if ((mode != MediaScheduledRtpPacketizationMode::H264AnnexB &&
         mode != MediaScheduledRtpPacketizationMode::HevcAnnexB) ||
        maximumDatagramBytes <= RtpHeaderBytes || authority.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "video RTP emission contract is incomplete"));
    }
    const auto codec = mode == MediaScheduledRtpPacketizationMode::H264AnnexB
        ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
    auto maximum =
        MediaDeterministicVideoRtpPacketizer::maximumDatagramsPerAccessUnit(
            maximumAccessUnitPayloadBytes, codec,
            maximumDatagramBytes - RtpHeaderBytes);
    if (!maximum) return Result::failure(maximum.error());
    return Result::success(MediaRtpAccessUnitEmissionContract(
        maximumAccessUnitPayloadBytes, maximum.value(), mode,
        maximumDatagramBytes, std::move(layout),
        std::move(authority)));
}

::media::Result<MediaRtpAccessUnitEmissionContract>
MediaRtpAccessUnitEmissionContract::createAacLatm(
    std::uint64_t maximumAccessUnitPayloadBytes,
    std::size_t maximumDatagramBytes,
    std::string authority)
{
    using Result = ::media::Result<MediaRtpAccessUnitEmissionContract>;
    if (maximumAccessUnitPayloadBytes == 0 ||
        maximumDatagramBytes <= RtpHeaderBytes || authority.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "AAC LATM RTP emission contract is incomplete"));
    }
    auto maximum = aacLatmDatagrams(
        maximumAccessUnitPayloadBytes, maximumDatagramBytes - RtpHeaderBytes);
    if (!maximum) return Result::failure(maximum.error());
    return Result::success(MediaRtpAccessUnitEmissionContract(
        maximumAccessUnitPayloadBytes, maximum.value(),
        MediaScheduledRtpPacketizationMode::AacLatm, maximumDatagramBytes,
        std::nullopt,
        std::move(authority)));
}

} // namespace media::ffmpeg::graph
