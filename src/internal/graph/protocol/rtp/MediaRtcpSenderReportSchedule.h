#pragma once

#include "internal/graph/time/MediaRunningTime.h"
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
        MediaRunningTime nextDeadline) noexcept;

    std::uint64_t m_generation;
    std::uint64_t m_revision;
    MediaRunningTime m_expectedDeadline;
    MediaRunningTime m_nextDeadline;
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
        MediaRunningTime initialDeadline,
        MediaRunningTime interval,
        MediaRunningTime maximumLateness,
        std::uint64_t generation) noexcept;

    ::media::Result<std::optional<MediaRtcpSenderReportScheduleDecision>>
    prepare(MediaRunningTime now, std::uint64_t generation) const noexcept;

    ::media::Status commit(
        const MediaRtcpSenderReportCommitToken& token) noexcept;

    ::media::Status reset(MediaRunningTime initialDeadline,
                          std::uint64_t generation) noexcept;

    MediaRunningTime nextDeadline() const noexcept { return m_nextDeadline; }
    MediaRunningTime interval() const noexcept { return m_interval; }
    MediaRunningTime maximumLateness() const noexcept
    {
        return m_maximumLateness;
    }
    std::uint64_t generation() const noexcept { return m_generation; }

private:
    MediaRtcpSenderReportSchedule(MediaRunningTime initialDeadline,
                                  MediaRunningTime interval,
                                  MediaRunningTime maximumLateness,
                                  std::uint64_t generation) noexcept;

    MediaRunningTime m_nextDeadline;
    MediaRunningTime m_interval;
    MediaRunningTime m_maximumLateness;
    std::uint64_t m_generation;
    std::uint64_t m_revision;
};

} // namespace media::ffmpeg::graph
