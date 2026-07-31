#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

struct MediaSdpWireSessionFields final {
    std::string_view originUsername;
    std::uint64_t sessionId;
    std::uint64_t sessionVersion;
    std::string_view sessionName;
    MediaIpAddressFamily addressFamily;
    std::string_view numericAddress;
};

struct MediaSdpWireMediaFields final {
    std::string_view mediaKind;
    std::uint16_t rtpPort;
    int payloadType;
    std::uint16_t rtcpPort;
    MediaIpAddressFamily rtcpAddressFamily;
    std::string_view rtcpNumericAddress;
};

class MediaSdpWireFormat final {
public:
    static ::media::Status appendSession(
        std::string& output,
        const MediaSdpWireSessionFields& fields);
    static ::media::Status appendMediaTransport(
        std::string& output,
        const MediaSdpWireMediaFields& fields);
    static void appendLine(
        std::string& output,
        std::string_view line);

private:
    MediaSdpWireFormat() = delete;
};

} // namespace media::ffmpeg::graph
