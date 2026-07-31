#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

class MediaMpegTsRtpDatagramSink;

class ScheduledRtpSenderCounters final {
public:
    static ::media::Result<ScheduledRtpSenderCounters> create(
        std::uint64_t packetCount,
        std::uint64_t octetCount) noexcept;

    std::uint64_t packetCount() const noexcept { return m_packetCount; }
    std::uint64_t octetCount() const noexcept { return m_octetCount; }

private:
    friend class ScheduledRtpSenderSession;
    friend class MediaMpegTsRtpDatagramSink;

    ScheduledRtpSenderCounters(std::uint64_t packetCount,
                               std::uint64_t octetCount) noexcept;
    ::media::Status preflight(std::size_t payloadOctets) const noexcept;
    void commit(std::size_t payloadOctets) noexcept;

    std::uint64_t m_packetCount;
    std::uint64_t m_octetCount;
};

class ScheduledRtpSenderConfig final {
public:
    static ::media::Result<ScheduledRtpSenderConfig> create(
        ScheduledRtpMuxStreamConfig streamConfig,
        MediaSharedNtpEpoch ntpEpoch,
        MediaRtpOutputClockMapper rtpMapper,
        MediaRtcpSenderReportSchedule senderReportSchedule,
        std::string cname,
        std::uint64_t scheduleGeneration,
        ScheduledRtpSenderCounters initialCounters);

    ScheduledRtpSenderConfig(ScheduledRtpSenderConfig&&) noexcept = default;
    ScheduledRtpSenderConfig& operator=(ScheduledRtpSenderConfig&&) noexcept = default;
    ScheduledRtpSenderConfig(const ScheduledRtpSenderConfig&) = delete;
    ScheduledRtpSenderConfig& operator=(const ScheduledRtpSenderConfig&) = delete;

    const ScheduledRtpMuxStreamConfig& streamConfig() const noexcept
    {
        return m_streamConfig;
    }
    const MediaSharedNtpEpoch& ntpEpoch() const noexcept { return m_ntpEpoch; }
    const MediaRtpOutputClockMapper& rtpMapper() const noexcept
    {
        return m_rtpMapper;
    }
    const MediaRtcpSenderReportSchedule& senderReportSchedule() const noexcept
    {
        return m_senderReportSchedule;
    }
    const std::string& cname() const noexcept { return m_cname; }
    std::uint64_t scheduleGeneration() const noexcept
    {
        return m_scheduleGeneration;
    }
    ScheduledRtpSenderCounters initialCounters() const noexcept
    {
        return m_initialCounters;
    }

private:
    friend class ScheduledRtpSenderSession;

    ScheduledRtpSenderConfig(
        ScheduledRtpMuxStreamConfig streamConfig,
        MediaSharedNtpEpoch ntpEpoch,
        MediaRtpOutputClockMapper rtpMapper,
        MediaRtcpSenderReportSchedule senderReportSchedule,
        std::string cname,
        std::uint64_t scheduleGeneration,
        ScheduledRtpSenderCounters initialCounters) noexcept;

    ScheduledRtpMuxStreamConfig m_streamConfig;
    MediaSharedNtpEpoch m_ntpEpoch;
    MediaRtpOutputClockMapper m_rtpMapper;
    MediaRtcpSenderReportSchedule m_senderReportSchedule;
    std::string m_cname;
    std::uint64_t m_scheduleGeneration;
    ScheduledRtpSenderCounters m_initialCounters;
};

} // namespace media::ffmpeg::graph
