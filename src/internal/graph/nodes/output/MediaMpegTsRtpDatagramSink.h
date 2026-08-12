#pragma once

#include "internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaMpegTsRtpDatagramSink final : public MediaTsDatagramSink {
public:
    static ::media::Result<std::unique_ptr<MediaMpegTsRtpDatagramSink>> create(
        const MediaMpegTsRtpOutputPlan& plan,
        const MediaProtocolOutputActivation& activation,
        const MediaSharedNtpEpoch& sharedNtpEpoch,
        std::shared_ptr<MediaMpegTsRtpContinuityState> continuity,
        MediaUdpDatagramSenderPortFactory& portFactory);
    ~MediaMpegTsRtpDatagramSink() noexcept override;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime emitOnMaster) override;
    ::media::Status flush() override;
    ::media::Status close() override;
    void abort() noexcept override;

private:
    MediaMpegTsRtpDatagramSink(
        std::unique_ptr<MediaRtpUdpSenderTransport> transport,
        MediaMpegTsRtpPacketizer packetizer,
        MediaSharedNtpEpoch ntpEpoch,
        MediaRtcpSenderReportSchedule senderReportSchedule,
        std::shared_ptr<MediaMpegTsRtpContinuityState> continuity,
        std::string cname,
        std::uint32_t ssrc,
        std::uint64_t generation) noexcept;

    ::media::Status dispatchSenderReport(
        MediaRunningTime now,
        const MediaMpegTsRtpCounterSnapshot& counters);
    ::media::Status sendTerminalReport();
    ::media::Status sendRtcp(
        std::span<const std::uint8_t> datagram) noexcept;
    ::media::Status fail(::media::ErrorInfo error) noexcept;
    ::media::Status terminalStatus() const;
    ::media::Status closeTransport() noexcept;
    void logContinuity(
        const char* stage,
        const MediaMpegTsRtpCounterSnapshot& counters,
        std::uint16_t sequenceNumber) const;

    std::unique_ptr<MediaRtpUdpSenderTransport> m_transport;
    MediaMpegTsRtpPacketizer m_packetizer;
    MediaSharedNtpEpoch m_ntpEpoch;
    MediaRtcpSenderReportSchedule m_senderReportSchedule;
    std::shared_ptr<MediaMpegTsRtpContinuityState> m_continuity;
    std::string m_cname;
    std::uint32_t m_ssrc;
    std::uint64_t m_generation;
    std::optional<MediaRunningTime> m_lastEmitOnMaster;
    std::optional<std::uint16_t> m_firstSequenceNumber;
    std::optional<std::uint16_t> m_lastSequenceNumber;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
