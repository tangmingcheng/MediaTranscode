#include "internal/graph/protocol/sdp/MediaRtpSdpSerializer.h"

#include <string>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {

const char* addressType(MediaIpAddressFamily family) noexcept
{
    switch (family) {
    case MediaIpAddressFamily::Ipv4:
        return "IP4";
    case MediaIpAddressFamily::Ipv6:
        return "IP6";
    }
    return nullptr;
}

void appendLine(std::string& output, std::string line)
{
    output += line;
    output += "\r\n";
}

} // namespace

::media::Result<std::string> MediaRtpSdpSerializer::serialize(
    const MediaRtpSdpDescription& description)
{
    const auto& session = description.session();
    const char* sessionAddressType = addressType(session.addressFamily());
    if (!sessionAddressType) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::internalError(
                "SDP session contains an unsupported IP address family"));
    }
    std::string output;
    output.reserve(1024);
    appendLine(output, "v=0");
    appendLine(output,
               "o=" + session.originUsername() + " " +
               std::to_string(session.sessionId()) + " " +
               std::to_string(session.sessionVersion()) + " IN " +
               sessionAddressType + std::string(" ") +
               session.numericAddress());
    appendLine(output, "s=" + session.sessionName());
    appendLine(output, "t=0 0");

    for (const auto& media : description.media()) {
        const auto& identity = media.identity();
        const char* mediaAddressType = addressType(identity.addressFamily());
        if (!mediaAddressType) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::internalError(
                    "SDP media contains an unsupported IP address family"));
        }
        const std::string kind = identity.kind() == MediaSdpMediaKind::Video
            ? "video" : "audio";
        appendLine(output,
                   "m=" + kind + " " + std::to_string(identity.remoteRtpPort()) +
                   " RTP/AVP " + std::to_string(identity.payloadType()));
        appendLine(output,
                   "c=IN " + std::string(mediaAddressType) +
                   " " + identity.remoteRtpNumericAddress());
        appendLine(output,
                   "a=rtcp:" + std::to_string(identity.remoteRtcpPort()) +
                   " IN " + mediaAddressType + std::string(" ") +
                   identity.remoteRtcpNumericAddress());
        std::visit([&](const auto& codec) {
            using Codec = std::decay_t<decltype(codec)>;
            if constexpr (std::is_same_v<Codec, MediaH264SdpCodecDescription>) {
                appendLine(output,
                           "a=rtpmap:" + std::to_string(identity.payloadType()) +
                           " H264/90000");
                appendLine(output,
                           "a=fmtp:" + std::to_string(identity.payloadType()) +
                           " packetization-mode=" +
                           std::to_string(codec.packetizationMode()) +
                           ";profile-level-id=" +
                           codec.profileLevelId() + ";sprop-parameter-sets=" +
                           codec.spropParameterSets());
            } else {
                appendLine(output,
                           "a=rtpmap:" + std::to_string(identity.payloadType()) +
                           " MP4A-LATM/" + std::to_string(codec.sampleRate()) +
                           "/" + std::to_string(codec.channels()));
                appendLine(output,
                           "a=fmtp:" + std::to_string(identity.payloadType()) +
                           " profile-level-id=" +
                           std::to_string(codec.profileLevelId()) +
                           ";cpresent=" +
                           std::to_string(codec.configurationPresent() ? 1 : 0) +
                           ";config=" + codec.streamMuxConfigHex());
            }
        }, media.codec());
        appendLine(output,
                   "a=ssrc:" + std::to_string(identity.ssrc()) +
                   " cname:" + session.cname());
    }
    return ::media::Result<std::string>::success(std::move(output));
}

} // namespace media::ffmpeg::graph
