#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"

#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/MediaUtf8TextValidator.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool hasForbiddenTextByte(const std::string& value) noexcept
{
    return value.empty() || std::any_of(value.begin(), value.end(), [](unsigned char byte) {
        return byte == 0 || byte == '\r' || byte == '\n' || byte < 0x20 || byte == 0x7f;
    });
}

bool isToken(const std::string& value) noexcept
{
    return !hasForbiddenTextByte(value) &&
           std::all_of(value.begin(), value.end(), [](unsigned char byte) {
               return byte > 0x20 && byte < 0x7f &&
                      byte != '(' && byte != ')' && byte != '<' && byte != '>' &&
                      byte != '@' && byte != ',' && byte != ';' && byte != ':' &&
                      byte != '\\' && byte != '"' && byte != '/' && byte != '[' &&
                      byte != ']' && byte != '?' && byte != '=' && byte != '{' && byte != '}';
           });
}

bool isUnsupportedIpv4Multicast(const MediaNumericIpAddress& address) noexcept
{
    return address.addressFamily() == MediaIpAddressFamily::Ipv4 &&
           address.isMulticast();
}

} // namespace

MediaH264SdpCodecDescription::MediaH264SdpCodecDescription(
    std::string profileLevelId, std::string spropParameterSets,
    int packetizationMode) noexcept
    : m_profileLevelId(std::move(profileLevelId)),
      m_spropParameterSets(std::move(spropParameterSets)),
      m_packetizationMode(packetizationMode)
{
}

MediaAacLatmSdpCodecDescription::MediaAacLatmSdpCodecDescription(
    int sampleRate, int channels, int profileLevelId,
    std::string streamMuxConfigHex, bool configurationPresent) noexcept
    : m_sampleRate(sampleRate), m_channels(channels),
      m_profileLevelId(profileLevelId),
      m_streamMuxConfigHex(std::move(streamMuxConfigHex)),
      m_configurationPresent(configurationPresent)
{
}

MediaHevcSdpCodecDescription::MediaHevcSdpCodecDescription(
    std::string spropVps,
    std::string spropSps,
    std::string spropPps) noexcept
    : m_spropVps(std::move(spropVps)),
      m_spropSps(std::move(spropSps)),
      m_spropPps(std::move(spropPps))
{
}

MediaSdpSessionIdentity::MediaSdpSessionIdentity(
    std::string originUsername,
    std::uint64_t sessionId,
    std::uint64_t sessionVersion,
    std::string sessionName,
    MediaNumericIpAddress originAddress,
    std::string cname) noexcept
    : m_originUsername(std::move(originUsername)), m_sessionId(sessionId),
      m_sessionVersion(sessionVersion), m_sessionName(std::move(sessionName)),
      m_originAddress(std::move(originAddress)),
      m_cname(std::move(cname))
{
}

::media::Result<MediaSdpSessionIdentity> MediaSdpSessionIdentity::create(
    std::string originUsername,
    std::uint64_t sessionId,
    std::uint64_t sessionVersion,
    std::string sessionName,
    MediaIpAddressFamily addressFamily,
    std::string numericAddress,
    std::string cname)
{
    auto originAddress = MediaNumericIpAddress::create(
        addressFamily, std::move(numericAddress));
    if (!isToken(originUsername) || originUsername.size() > 255 ||
        !MediaUtf8TextValidator::validateNonControlText(
            sessionName, 255, "SDP session name") ||
        !MediaRtcpSdesTextValidator::validateCname(cname) ||
        !originAddress || originAddress.value().isMulticast()) {
        return ::media::Result<MediaSdpSessionIdentity>::failure(
            ::media::ErrorInfo::invalidArgument("invalid complete SDP session identity"));
    }
    return ::media::Result<MediaSdpSessionIdentity>::success(
        MediaSdpSessionIdentity(
            std::move(originUsername), sessionId, sessionVersion,
            std::move(sessionName), std::move(originAddress.value()),
            std::move(cname)));
}

MediaRtpSdpMediaIdentity::MediaRtpSdpMediaIdentity(
    MediaSdpMediaKind kind,
    MediaNumericIpAddress remoteRtpAddress,
    MediaNumericIpAddress remoteRtcpAddress,
    std::uint16_t remoteRtpPort,
    std::uint16_t remoteRtcpPort,
    std::uint8_t payloadType,
    std::uint32_t ssrc,
    int clockRate,
    int channels) noexcept
    : m_kind(kind), m_remoteRtpAddress(std::move(remoteRtpAddress)),
      m_remoteRtcpAddress(std::move(remoteRtcpAddress)),
      m_remoteRtpPort(remoteRtpPort), m_remoteRtcpPort(remoteRtcpPort),
      m_payloadType(payloadType), m_ssrc(ssrc), m_clockRate(clockRate),
      m_channels(channels)
{
}

::media::Result<MediaRtpSdpMediaIdentity> MediaRtpSdpMediaIdentity::create(
    MediaSdpMediaKind kind,
    MediaIpAddressFamily addressFamily,
    std::string remoteRtpNumericAddress,
    std::string remoteRtcpNumericAddress,
    std::uint16_t remoteRtpPort,
    std::uint16_t remoteRtcpPort,
    std::uint8_t payloadType,
    std::uint32_t ssrc,
    int clockRate,
    int channels)
{
    auto rtpAddress = MediaNumericIpAddress::create(
        addressFamily, std::move(remoteRtpNumericAddress));
    auto rtcpAddress = MediaNumericIpAddress::create(
        addressFamily, std::move(remoteRtcpNumericAddress));
    const bool validKindClock =
        (kind == MediaSdpMediaKind::Video && clockRate == 90'000 && channels == 0) ||
        (kind == MediaSdpMediaKind::Audio && clockRate > 0 &&
         (channels == 1 || channels == 2));
    if (!rtpAddress || !rtcpAddress ||
        (rtpAddress && isUnsupportedIpv4Multicast(rtpAddress.value())) ||
        (rtcpAddress && isUnsupportedIpv4Multicast(rtcpAddress.value())) ||
        remoteRtpPort == 0 || remoteRtcpPort == 0 ||
        (rtpAddress && rtcpAddress && rtpAddress.value() == rtcpAddress.value() &&
         remoteRtpPort == remoteRtcpPort) ||
        payloadType < 96 || payloadType > 127 ||
        ssrc == 0 || !validKindClock) {
        return ::media::Result<MediaRtpSdpMediaIdentity>::failure(
            ::media::ErrorInfo::invalidArgument("invalid complete RTP SDP media identity"));
    }
    return ::media::Result<MediaRtpSdpMediaIdentity>::success(
        MediaRtpSdpMediaIdentity(
            kind, std::move(rtpAddress.value()), std::move(rtcpAddress.value()),
            remoteRtpPort,
            remoteRtcpPort, payloadType, ssrc, clockRate, channels));
}

MediaRtpSdpMediaDescription::MediaRtpSdpMediaDescription(
    MediaRtpSdpMediaIdentity identity,
    MediaSdpCodecDescription codec) noexcept
    : m_identity(std::move(identity)), m_codec(std::move(codec))
{
}

::media::Result<MediaRtpSdpMediaDescription>
MediaRtpSdpMediaDescription::create(
    MediaRtpSdpMediaIdentity identity,
    MediaSdpCodecDescription codec)
{
    const bool matches = std::visit([&identity](const auto& typedCodec) {
        using Codec = std::decay_t<decltype(typedCodec)>;
        if constexpr (std::is_same_v<Codec, MediaH264SdpCodecDescription> ||
                      std::is_same_v<Codec, MediaHevcSdpCodecDescription>) {
            return identity.kind() == MediaSdpMediaKind::Video &&
                   identity.clockRate() == 90'000 && identity.channels() == 0;
        } else {
            return identity.kind() == MediaSdpMediaKind::Audio &&
                   identity.clockRate() == typedCodec.sampleRate() &&
                   identity.channels() == typedCodec.channels();
        }
    }, codec);
    if (!matches) {
        return ::media::Result<MediaRtpSdpMediaDescription>::failure(
            ::media::ErrorInfo::invalidArgument(
                "planned RTP media identity does not match final codec facts"));
    }
    return ::media::Result<MediaRtpSdpMediaDescription>::success(
        MediaRtpSdpMediaDescription(std::move(identity), std::move(codec)));
}

MediaRtpSdpDescription::MediaRtpSdpDescription(
    MediaSdpSessionIdentity session,
    std::vector<MediaRtpSdpMediaDescription> media) noexcept
    : m_session(std::move(session)), m_media(std::move(media))
{
}

::media::Result<MediaRtpSdpDescription> MediaRtpSdpDescription::create(
    MediaSdpSessionIdentity session,
    std::vector<MediaRtpSdpMediaDescription> media)
{
    const bool videoOnly = media.size() == 1 &&
                           media[0].identity().kind() == MediaSdpMediaKind::Video;
    const bool av = media.size() == 2 &&
                    media[0].identity().kind() == MediaSdpMediaKind::Video &&
                    media[1].identity().kind() == MediaSdpMediaKind::Audio;
    bool consistent = videoOnly || av;
    using Endpoint = std::pair<MediaNumericIpAddress, std::uint16_t>;
    std::vector<Endpoint> endpoints;
    for (const auto& item : media) {
        const auto rtp = Endpoint{
            item.identity().remoteRtpAddress(), item.identity().remoteRtpPort()};
        const auto rtcp = Endpoint{
            item.identity().remoteRtcpAddress(), item.identity().remoteRtcpPort()};
        consistent = consistent &&
                     item.identity().addressFamily() == session.addressFamily() &&
                     item.identity().remoteRtpNumericAddress() ==
                         session.numericAddress() &&
                     std::find(endpoints.begin(), endpoints.end(), rtp) == endpoints.end() &&
                     std::find(endpoints.begin(), endpoints.end(), rtcp) == endpoints.end();
        endpoints.push_back(rtp);
        endpoints.push_back(rtcp);
    }
    if (av) {
        consistent = consistent &&
                     media[0].identity().payloadType() != media[1].identity().payloadType() &&
                     media[0].identity().ssrc() != media[1].identity().ssrc();
    }
    if (!consistent) {
        return ::media::Result<MediaRtpSdpDescription>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP SDP requires a shared session connection address and "
                "ordered distinct video and audio media"));
    }
    return ::media::Result<MediaRtpSdpDescription>::success(
        MediaRtpSdpDescription(std::move(session), std::move(media)));
}

} // namespace media::ffmpeg::graph
