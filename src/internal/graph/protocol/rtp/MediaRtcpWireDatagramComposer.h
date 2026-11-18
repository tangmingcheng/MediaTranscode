#pragma once
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"

namespace media::ffmpeg::graph {
class MediaRtcpWireDatagramComposer final {
public:
    static ::media::Result<MediaRtcpSenderReportPlan> prepare(
        std::uint32_t ssrc, std::string_view cname, MediaRunningTime reportInstant,
        const MediaSharedNtpEpoch& ntpEpoch, const MediaRtpOutputClockMapper& clockMapper,
        std::uint64_t packetCount, std::uint64_t octetCount, bool includeBye);
private:
    MediaRtcpWireDatagramComposer() = delete;
};
} // namespace media::ffmpeg::graph
