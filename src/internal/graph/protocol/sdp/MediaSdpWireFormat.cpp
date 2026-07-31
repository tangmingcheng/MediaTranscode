#include "internal/graph/protocol/sdp/MediaSdpWireFormat.h"

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

} // namespace

void MediaSdpWireFormat::appendLine(
    std::string& output,
    std::string_view line)
{
    output.append(line);
    output.append("\r\n");
}

::media::Status MediaSdpWireFormat::appendSession(
    std::string& output,
    const MediaSdpWireSessionFields& fields)
{
    const char* type = addressType(fields.addressFamily);
    if (!type) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "SDP session contains an unsupported address family"));
    }
    appendLine(output, "v=0");
    appendLine(
        output,
        "o=" + std::string(fields.originUsername) + " " +
            std::to_string(fields.sessionId) + " " +
            std::to_string(fields.sessionVersion) + " IN " + type +
            " " + std::string(fields.numericAddress));
    appendLine(output, "s=" + std::string(fields.sessionName));
    appendLine(
        output,
        "c=IN " + std::string(type) + " " +
            std::string(fields.numericAddress));
    appendLine(output, "t=0 0");
    return ::media::Status::success();
}

::media::Status MediaSdpWireFormat::appendMediaTransport(
    std::string& output,
    const MediaSdpWireMediaFields& fields)
{
    const char* rtcpType = addressType(fields.rtcpAddressFamily);
    if (!rtcpType) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "SDP media contains an unsupported address family"));
    }
    appendLine(
        output,
        "m=" + std::string(fields.mediaKind) + " " +
            std::to_string(fields.rtpPort) + " RTP/AVP " +
            std::to_string(fields.payloadType));
    appendLine(
        output,
        "a=rtcp:" + std::to_string(fields.rtcpPort) +
            " IN " + rtcpType + " " +
            std::string(fields.rtcpNumericAddress));
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
