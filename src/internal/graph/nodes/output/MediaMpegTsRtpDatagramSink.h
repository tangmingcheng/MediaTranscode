#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaMpegTsRtpDatagramSink final : public MediaTsDatagramSink {
public:
    static ::media::Result<std::unique_ptr<MediaMpegTsRtpDatagramSink>> create(
        const MediaMpegTsRtpOutputPlan& plan,
        const MediaPlaybackEpoch& epoch,
        const MediaSharedNtpEpoch& sharedNtpEpoch,
        MediaUdpDatagramSenderPortFactory& portFactory);
    ~MediaMpegTsRtpDatagramSink() override;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime emitOnMaster) override;
    ::media::Status flush() override;
    ::media::Status close() override;

private:
    MediaMpegTsRtpDatagramSink(
        std::unique_ptr<MediaRtpUdpSenderTransport> transport,
        MediaMpegTsRtpPacketizer packetizer,
        MediaSharedNtpEpoch ntpEpoch,
        MediaRtcpSenderReportSchedule senderReportSchedule,
        ScheduledRtpSenderCounters counters,
        std::string cname,
        std::uint32_t ssrc,
        std::uint64_t generation) noexcept;

    ::media::Status dispatchSenderReport(MediaRunningTime now);
    ::media::Status sendTerminalReport();
    ::media::Status sendRtcp(
        std::span<const std::uint8_t> datagram) noexcept;
    ::media::Status fail(::media::ErrorInfo error) noexcept;
    ::media::Status terminalStatus() const;
    void closeTransport() noexcept;

    std::unique_ptr<MediaRtpUdpSenderTransport> m_transport;
    MediaMpegTsRtpPacketizer m_packetizer;
    MediaSharedNtpEpoch m_ntpEpoch;
    MediaRtcpSenderReportSchedule m_senderReportSchedule;
    ScheduledRtpSenderCounters m_counters;
    std::string m_cname;
    std::uint32_t m_ssrc;
    std::uint64_t m_generation;
    std::optional<MediaRunningTime> m_lastEmitOnMaster;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
