#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph {

class MediaRtcpWireDatagramComposer final {
public:
    static ::media::Result<std::vector<std::uint8_t>> composeSenderReport(
        std::uint32_t ssrc,
        std::string_view cname,
        const MediaRtcpSenderReportScheduleDecision& decision,
        const MediaSharedNtpEpoch& ntpEpoch,
        const MediaRtpOutputClockMapper& clockMapper,
        std::uint64_t packetCount,
        std::uint64_t octetCount);

    static ::media::Result<std::vector<std::uint8_t>> composeTerminalReport(
        std::uint32_t ssrc,
        std::string_view cname,
        MediaRunningTime reportInstant,
        const MediaSharedNtpEpoch& ntpEpoch,
        const MediaRtpOutputClockMapper& clockMapper,
        std::uint64_t packetCount,
        std::uint64_t octetCount);

private:
    MediaRtcpWireDatagramComposer() = delete;
};

} // namespace media::ffmpeg::graph
