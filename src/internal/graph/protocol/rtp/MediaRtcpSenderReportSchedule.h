#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/protocol/rtp/MediaRtcpReportingPolicy.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtcpSenderReportSchedule;

class MediaRtcpSenderReportCommitToken final {
public:
    std::uint64_t generation() const noexcept { return m_generation; }
    MediaRunningTime expectedDeadline() const noexcept
    {
        return m_expectedDeadline;
    }
    MediaRunningTime nextDeadline() const noexcept { return m_nextDeadline; }

private:
    friend class MediaRtcpSenderReportSchedule;

    MediaRtcpSenderReportCommitToken(
        std::uint64_t generation,
        std::uint64_t revision,
        MediaRunningTime expectedDeadline,
        MediaRunningTime nextDeadline,
        std::uint64_t nextRandomState) noexcept;

    std::uint64_t m_generation;
    std::uint64_t m_revision;
    MediaRunningTime m_expectedDeadline;
    MediaRunningTime m_nextDeadline;
    std::uint64_t m_nextRandomState;
};

struct MediaRtcpSenderReportScheduleDecision final {
    MediaRtcpSenderReportScheduleDecision() = delete;
    MediaRtcpSenderReportScheduleDecision(
        MediaRunningTime scheduledDeadline,
        MediaRunningTime reportInstant,
        MediaRunningTime nextDeadline,
        MediaRunningTime lateness,
        std::uint64_t skippedIntervals,
        MediaRtcpSenderReportCommitToken commitToken);

    MediaRunningTime scheduledDeadline;
    MediaRunningTime reportInstant;
    MediaRunningTime nextDeadline;
    MediaRunningTime lateness;
    std::uint64_t skippedIntervals;
    MediaRtcpSenderReportCommitToken commitToken;
};

class MediaRtcpSenderReportSchedule final {
public:
    static ::media::Result<MediaRtcpSenderReportSchedule> create(
        MediaRunningTime activation,
        MediaRtcpReportingPolicy reportingPolicy,
        MediaRunningTime maximumLateness,
        std::uint64_t generation,
        std::uint64_t randomSeed) noexcept;

    ::media::Result<std::optional<MediaRtcpSenderReportScheduleDecision>>
    prepare(MediaRunningTime now, std::uint64_t generation) const noexcept;

    ::media::Status commit(
        const MediaRtcpSenderReportCommitToken& token) noexcept;

    ::media::Status reset(MediaRunningTime activation,
                          std::uint64_t generation,
                          std::uint64_t randomSeed) noexcept;

    MediaRunningTime nextDeadline() const noexcept { return m_nextDeadline; }
    const MediaRtcpReportingPolicy& reportingPolicy() const noexcept
    {
        return m_reportingPolicy;
    }
    MediaRunningTime maximumLateness() const noexcept
    {
        return m_maximumLateness;
    }
    std::uint64_t generation() const noexcept { return m_generation; }

private:
    MediaRtcpSenderReportSchedule(MediaRunningTime initialDeadline,
                                  MediaRtcpReportingPolicy reportingPolicy,
                                  MediaRunningTime maximumLateness,
                                  std::uint64_t generation,
                                  std::uint64_t randomState) noexcept;

    MediaRunningTime m_nextDeadline;
    MediaRtcpReportingPolicy m_reportingPolicy;
    MediaRunningTime m_maximumLateness;
    std::uint64_t m_generation;
    std::uint64_t m_revision;
    std::uint64_t m_randomState;
};

} // namespace media::ffmpeg::graph
