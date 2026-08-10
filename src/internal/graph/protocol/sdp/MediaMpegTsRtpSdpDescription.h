#pragma once

#include "internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

class MediaMpegTsRtpSdpDescription final {
public:
    static ::media::Result<MediaMpegTsRtpSdpDescription> create(
        const MediaMpegTsRtpOutputPlan& plan,
        const MediaSharedNtpEpoch& sharedNtpEpoch,
        const MediaProtocolOutputActivation& activation);

    ::media::Result<std::string> serialize() const;
    const std::string& path() const noexcept { return m_path; }

private:
    MediaMpegTsRtpSdpDescription(
        std::string path,
        std::string originUsername,
        std::uint64_t sessionId,
        std::uint64_t sessionVersion,
        std::string sessionName,
        MediaIpAddressFamily addressFamily,
        std::string numericAddress,
        std::uint16_t rtpPort,
        std::uint16_t rtcpPort,
        int payloadType,
        int clockRate,
        std::uint32_t ssrc,
        std::string cname) noexcept;

    std::string m_path;
    std::string m_originUsername;
    std::uint64_t m_sessionId;
    std::uint64_t m_sessionVersion;
    std::string m_sessionName;
    MediaIpAddressFamily m_addressFamily;
    std::string m_numericAddress;
    std::uint16_t m_rtpPort;
    std::uint16_t m_rtcpPort;
    int m_payloadType;
    int m_clockRate;
    std::uint32_t m_ssrc;
    std::string m_cname;
};

} // namespace media::ffmpeg::graph
