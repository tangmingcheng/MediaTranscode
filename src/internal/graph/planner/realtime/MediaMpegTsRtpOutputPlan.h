#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/planner/realtime/MediaScheduledDatagramPacingPlan.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaMpegTsRtpSdpPlan final {
    std::string path;
    std::string originUsername;
    std::string sessionName;
    MediaIpAddressFamily originAddressFamily;
    std::string originNumericAddress;
    std::string cname;
};

class MediaMpegTsRtpOutputPlan final {
public:
    static ::media::Result<MediaMpegTsRtpOutputPlan> create(
        MediaRtpUdpSenderConfig transport,
        std::string sdpPath,
        std::string sessionIdentity,
        MediaRunningTime senderReportInterval,
        MediaScheduledDatagramPacingPlan pacing);

    MediaMpegTsRtpOutputPlan(MediaMpegTsRtpOutputPlan&&) noexcept = default;
    MediaMpegTsRtpOutputPlan& operator=(
        MediaMpegTsRtpOutputPlan&&) noexcept = default;
    MediaMpegTsRtpOutputPlan(const MediaMpegTsRtpOutputPlan&) = delete;
    MediaMpegTsRtpOutputPlan& operator=(
        const MediaMpegTsRtpOutputPlan&) = delete;

    ::media::Result<MediaMpegTsRtpOutputPlan> clone() const;
    const MediaRtpUdpSenderConfig& transport() const noexcept;
    int payloadType() const noexcept;
    int clockRate() const noexcept;
    std::uint32_t ssrc() const noexcept;
    std::uint32_t baseTimestamp() const noexcept;
    std::uint16_t initialSequenceNumber() const noexcept;
    const std::string& cname() const noexcept;
    MediaRunningTime senderReportInterval() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::uint8_t tsPacketsPerPayload() const noexcept;
    const MediaMpegTsRtpSdpPlan& sdp() const noexcept;
    const MediaScheduledDatagramPacingPlan& pacing() const noexcept;

private:
    MediaMpegTsRtpOutputPlan(
        MediaRtpUdpSenderConfig transport,
        int payloadType,
        int clockRate,
        std::uint32_t ssrc,
        std::uint32_t baseTimestamp,
        std::uint16_t initialSequenceNumber,
        std::string cname,
        MediaRunningTime senderReportInterval,
        std::size_t maximumDatagramBytes,
        std::uint8_t tsPacketsPerPayload,
        MediaMpegTsRtpSdpPlan sdp,
        MediaScheduledDatagramPacingPlan pacing) noexcept;

    MediaRtpUdpSenderConfig m_transport;
    int m_payloadType;
    int m_clockRate;
    std::uint32_t m_ssrc;
    std::uint32_t m_baseTimestamp;
    std::uint16_t m_initialSequenceNumber;
    std::string m_cname;
    MediaRunningTime m_senderReportInterval;
    std::size_t m_maximumDatagramBytes;
    std::uint8_t m_tsPacketsPerPayload;
    MediaMpegTsRtpSdpPlan m_sdp;
    MediaScheduledDatagramPacingPlan m_pacing;
};

} // namespace media::ffmpeg::graph
