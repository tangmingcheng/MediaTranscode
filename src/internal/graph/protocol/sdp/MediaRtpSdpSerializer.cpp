#include "internal/graph/protocol/sdp/MediaRtpSdpSerializer.h"
#include "internal/graph/protocol/sdp/MediaSdpWireFormat.h"

#include <string>
#include <type_traits>

namespace media::ffmpeg::graph {
::media::Result<std::string> MediaRtpSdpSerializer::serialize(
    const MediaRtpSdpDescription& description)
{
    const auto& session = description.session();
    std::string output;
    output.reserve(1024);
    auto sessionWire = MediaSdpWireFormat::appendSession(
        output,
        {session.originUsername(), session.sessionId(),
         session.sessionVersion(), session.sessionName(),
         session.addressFamily(), session.numericAddress()});
    if (!sessionWire) {
        return ::media::Result<std::string>::failure(
            sessionWire.error());
    }

    for (const auto& media : description.media()) {
        const auto& identity = media.identity();
        const std::string_view kind =
            identity.kind() == MediaSdpMediaKind::Video
            ? "video" : "audio";
        auto mediaWire = MediaSdpWireFormat::appendMediaTransport(
            output,
            {kind, identity.remoteRtpPort(), identity.payloadType(),
             identity.remoteRtcpPort(), identity.addressFamily(),
             identity.remoteRtcpNumericAddress()});
        if (!mediaWire) {
            return ::media::Result<std::string>::failure(
                mediaWire.error());
        }
        std::visit([&](const auto& codec) {
            using Codec = std::decay_t<decltype(codec)>;
            if constexpr (std::is_same_v<Codec, MediaH264SdpCodecDescription>) {
                MediaSdpWireFormat::appendLine(output,
                           "a=rtpmap:" + std::to_string(identity.payloadType()) +
                           " H264/90000");
                MediaSdpWireFormat::appendLine(output,
                           "a=fmtp:" + std::to_string(identity.payloadType()) +
                           " packetization-mode=" +
                           std::to_string(codec.packetizationMode()) +
                           ";profile-level-id=" +
                           codec.profileLevelId() + ";sprop-parameter-sets=" +
                           codec.spropParameterSets());
            } else {
                MediaSdpWireFormat::appendLine(output,
                           "a=rtpmap:" + std::to_string(identity.payloadType()) +
                           " MP4A-LATM/" + std::to_string(codec.sampleRate()) +
                           "/" + std::to_string(codec.channels()));
                MediaSdpWireFormat::appendLine(output,
                           "a=fmtp:" + std::to_string(identity.payloadType()) +
                           " profile-level-id=" +
                           std::to_string(codec.profileLevelId()) +
                           ";cpresent=" +
                           std::to_string(codec.configurationPresent() ? 1 : 0) +
                           ";config=" + codec.streamMuxConfigHex());
            }
        }, media.codec());
        MediaSdpWireFormat::appendLine(output,
                   "a=ssrc:" + std::to_string(identity.ssrc()) +
                   " cname:" + session.cname());
    }
    return ::media::Result<std::string>::success(std::move(output));
}

} // namespace media::ffmpeg::graph
