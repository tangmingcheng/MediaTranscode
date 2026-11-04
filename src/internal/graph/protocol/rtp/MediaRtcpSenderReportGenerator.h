#pragma once

#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string_view>
#include <span>
#include <cstddef>

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
        std::string_view cname,
        MediaRtcpSenderReportTimestamp timestamp,
        std::uint64_t senderPacketCount,
        std::uint64_t senderOctetCount);

    std::uint32_t ssrc;
    std::string_view cname;
    MediaRtcpSenderReportTimestamp timestamp;
    std::uint64_t senderPacketCount;
    std::uint64_t senderOctetCount;
};

// CNAME storage is borrowed and must remain stable until synchronous writeInto.
class MediaRtcpSenderReportPlan final {
public:
    std::size_t bytes() const noexcept { return m_bytes; }
private:
    friend class MediaRtcpSenderReportGenerator;
    MediaRtcpSenderReportPlan(MediaRtcpSenderReportParameters parameters,
        std::size_t sdesBytes, std::size_t bytes, bool includeBye) noexcept
        : m_parameters(parameters), m_sdesBytes(sdesBytes), m_bytes(bytes), m_includeBye(includeBye) {}
    MediaRtcpSenderReportParameters m_parameters;
    std::size_t m_sdesBytes;
    std::size_t m_bytes;
    bool m_includeBye;
};
class MediaRtcpSenderReportGenerator final {
public:
    static ::media::Result<MediaRtcpSenderReportTimestamp> mapTimestamp(
        MediaRunningTime masterInstant,
        const MediaSharedNtpEpoch& ntpEpoch,
        const MediaRtpOutputClockMapper& rtpMapper) noexcept;

    static ::media::Result<MediaRtcpSenderReportPlan> prepare(
        const MediaRtcpSenderReportParameters& parameters, bool includeBye);
    static ::media::Status writeInto(const MediaRtcpSenderReportPlan& plan,
        std::span<std::uint8_t> output);

private:
    MediaRtcpSenderReportGenerator() = delete;
};

} // namespace media::ffmpeg::graph
