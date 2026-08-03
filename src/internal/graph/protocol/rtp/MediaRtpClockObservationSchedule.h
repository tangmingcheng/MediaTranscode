#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaRtpClockAgeTransition {
    Degraded,
    Expired
};

class MediaRtpClockObservationSchedule final {
public:
    static ::media::Result<MediaRtpClockObservationSchedule> create(
        std::int64_t senderReportTimeoutNs,
        std::int64_t maximumExtrapolationNs,
        std::int64_t cnameTimeoutNs);

    ::media::Status observeEvidence(std::int64_t senderReportObservedAtNs,
                                    std::int64_t cnameObservedAtNs) noexcept;
    std::optional<std::int64_t> nextDeadlineNs() const noexcept;
    ::media::Result<int> receiveTimeoutMs(std::int64_t observedAtNs,
                                          int plannedMaximumMs) const noexcept;
    ::media::Result<std::optional<MediaRtpClockAgeTransition>> transition(
        std::int64_t observedAtNs) noexcept;
    std::int64_t senderReportTimeoutNs() const noexcept;
    std::int64_t maximumExtrapolationNs() const noexcept;
    std::int64_t cnameTimeoutNs() const noexcept;
    void reset() noexcept;

private:
    MediaRtpClockObservationSchedule(std::int64_t senderReportTimeoutNs,
                                     std::int64_t maximumExtrapolationNs,
                                     std::int64_t cnameTimeoutNs) noexcept;

    std::int64_t m_senderReportTimeoutNs;
    std::int64_t m_maximumExtrapolationNs;
    std::int64_t m_cnameTimeoutNs;
    std::optional<std::int64_t> m_senderReportObservedAtNs;
    std::optional<std::int64_t> m_cnameObservedAtNs;
    bool m_degradedPublished = false;
    bool m_expiredPublished = false;
};

} // namespace media::ffmpeg::graph
