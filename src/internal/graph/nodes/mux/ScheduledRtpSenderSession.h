#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

struct AVPacket;

namespace media::ffmpeg::graph {

using ScheduledRtpDatagramSink = ScheduledRtpRewrittenDatagramSink;
using ScheduledRtcpDatagramSink =
    std::function<::media::Status(std::span<const std::uint8_t>)>;

enum class ScheduledRtcpDispatchKind {
    NotDue,
    Sent
};

class ScheduledRtcpDispatchDiagnostics final {
public:
    MediaRunningTime scheduledDeadline() const noexcept
    {
        return m_scheduledDeadline;
    }
    MediaRunningTime reportInstant() const noexcept { return m_reportInstant; }
    MediaRunningTime nextDeadline() const noexcept { return m_nextDeadline; }
    MediaRunningTime lateness() const noexcept { return m_lateness; }
    std::uint64_t skippedIntervals() const noexcept
    {
        return m_skippedIntervals;
    }
    MediaNtpWireTimestamp ntpTimestamp() const noexcept
    {
        return m_ntpTimestamp;
    }
    MediaRtpTimestamp rtpTimestamp() const noexcept { return m_rtpTimestamp; }
    ScheduledRtpSenderCounters senderCounters() const noexcept
    {
        return m_senderCounters;
    }

private:
    friend class ScheduledRtpSenderSession;

    ScheduledRtcpDispatchDiagnostics(
        MediaRunningTime scheduledDeadline,
        MediaRunningTime reportInstant,
        MediaRunningTime nextDeadline,
        MediaRunningTime lateness,
        std::uint64_t skippedIntervals,
        MediaNtpWireTimestamp ntpTimestamp,
        MediaRtpTimestamp rtpTimestamp,
        ScheduledRtpSenderCounters senderCounters) noexcept;

    MediaRunningTime m_scheduledDeadline;
    MediaRunningTime m_reportInstant;
    MediaRunningTime m_nextDeadline;
    MediaRunningTime m_lateness;
    std::uint64_t m_skippedIntervals;
    MediaNtpWireTimestamp m_ntpTimestamp;
    MediaRtpTimestamp m_rtpTimestamp;
    ScheduledRtpSenderCounters m_senderCounters;
};

class ScheduledRtcpDispatchResult final {
public:
    ScheduledRtcpDispatchKind kind() const noexcept { return m_kind; }
    MediaRunningTime nextDeadline() const noexcept { return m_nextDeadline; }
    const ScheduledRtcpDispatchDiagnostics* sentDiagnostics() const noexcept
    {
        return m_sent ? &*m_sent : nullptr;
    }

private:
    friend class ScheduledRtpSenderSession;

    explicit ScheduledRtcpDispatchResult(MediaRunningTime nextDeadline) noexcept;
    explicit ScheduledRtcpDispatchResult(
        ScheduledRtcpDispatchDiagnostics diagnostics);

    ScheduledRtcpDispatchKind m_kind;
    MediaRunningTime m_nextDeadline;
    std::optional<ScheduledRtcpDispatchDiagnostics> m_sent;
};

class ScheduledRtpSenderSession final {
public:
    static ::media::Result<std::unique_ptr<ScheduledRtpSenderSession>> create(
        ScheduledRtpSenderConfig config,
        ScheduledRtpDatagramSink rtpSink,
        ScheduledRtcpDatagramSink rtcpSink,
        ScheduledRtpPacketizerFactory& packetizerFactory);

    ScheduledRtpSenderSession(const ScheduledRtpSenderSession&) = delete;
    ScheduledRtpSenderSession& operator=(const ScheduledRtpSenderSession&) = delete;

    ::media::Status open();
    ::media::Status sendAccessUnit(
        const AVPacket& packet,
        MediaRunningTime presentationOnMaster);
    ::media::Result<ScheduledRtcpDispatchResult> dispatchSenderReport(
        MediaRunningTime now);

    const ScheduledRtpSenderCounters& counters() const noexcept
    {
        return m_counters;
    }

private:
    enum class State {
        Created,
        Open,
        Poisoned
    };

    class OperationGuard final {
    public:
        explicit OperationGuard(bool& operationActive) noexcept;
        ~OperationGuard();

        bool entered() const noexcept { return m_entered; }

    private:
        bool& m_operationActive;
        bool m_entered;
    };

    ScheduledRtpSenderSession(
        ScheduledRtpSenderConfig config,
        ScheduledRtpDatagramSink rtpSink,
        ScheduledRtcpDatagramSink rtcpSink);

    ::media::Status acceptRtpDatagram(
        std::span<const std::uint8_t> datagram,
        std::size_t payloadOctets);
    void poison(::media::ErrorInfo error);
    ::media::Status operationRejected() const;

    MediaSharedNtpEpoch m_ntpEpoch;
    MediaRtpOutputClockMapper m_rtpMapper;
    MediaRtcpSenderReportSchedule m_senderReportSchedule;
    std::string m_cname;
    std::uint64_t m_scheduleGeneration;
    std::uint32_t m_ssrc;
    ScheduledRtpSenderCounters m_counters;
    ScheduledRtpDatagramSink m_rtpSink;
    ScheduledRtcpDatagramSink m_rtcpSink;
    std::unique_ptr<ScheduledRtpPacketizerSession> m_packetizer;
    std::optional<ScheduledRtpMuxStreamConfig> m_pendingStreamConfig;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    bool m_operationActive = false;
    State m_state = State::Created;
};

} // namespace media::ffmpeg::graph
