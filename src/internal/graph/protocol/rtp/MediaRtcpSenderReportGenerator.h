#pragma once

#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaRtcpSenderReportTimestamp final {
public:
    MediaRunningTime masterInstant() const noexcept
    {
        return m_masterInstant;
    }
    const MediaNtpTimestamp& ntp() const noexcept { return m_ntp; }
    const MediaRtpTimestamp& rtp() const noexcept { return m_rtp; }

private:
    friend class MediaRtcpSenderReportGenerator;

    MediaRtcpSenderReportTimestamp(MediaRunningTime masterInstant,
                                   MediaNtpTimestamp ntp,
                                   MediaRtpTimestamp rtp) noexcept;

    MediaRunningTime m_masterInstant;
    MediaNtpTimestamp m_ntp;
    MediaRtpTimestamp m_rtp;
};

struct MediaRtcpSenderReportParameters final {
    MediaRtcpSenderReportParameters() = delete;
    MediaRtcpSenderReportParameters(
        std::uint32_t ssrc,
        std::string cname,
        MediaRtcpSenderReportTimestamp timestamp,
        std::uint64_t senderPacketCount,
        std::uint64_t senderOctetCount);

    std::uint32_t ssrc;
    std::string cname;
    MediaRtcpSenderReportTimestamp timestamp;
    std::uint64_t senderPacketCount;
    std::uint64_t senderOctetCount;
};

class MediaRtcpSenderReportGenerator final {
public:
    static ::media::Result<MediaRtcpSenderReportTimestamp> mapTimestamp(
        MediaRunningTime masterInstant,
        const MediaSharedNtpEpoch& ntpEpoch,
        const MediaRtpOutputClockMapper& rtpMapper) noexcept;

    static ::media::Result<std::vector<std::uint8_t>> serialize(
        const MediaRtcpSenderReportParameters& parameters);

    static ::media::Result<std::vector<std::uint8_t>> serializeWithBye(
        const MediaRtcpSenderReportParameters& parameters);

private:
    MediaRtcpSenderReportGenerator() = delete;
};

} // namespace media::ffmpeg::graph
