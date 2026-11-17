#pragma once
#include "internal/graph/time/MediaSteadyClock.h"
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {
// Stack-owned by the caller. Platform-specific fields remain absent elsewhere.
struct MediaNodeWaitObservation final {
    std::optional<std::int64_t> entered;
    std::optional<std::int64_t> returned;
    std::optional<std::int64_t> armBefore;
    std::optional<std::int64_t> armAfter;
    std::optional<std::int64_t> apiBefore;
    std::optional<std::int64_t> apiReturned;
    std::optional<std::int64_t> windowsDue100ns;
    std::optional<std::uint32_t> windowsWaitResult;
    std::optional<std::uint32_t> windowsError;
    std::optional<std::uint32_t> windowsProcessId;
    std::optional<std::uint32_t> windowsThreadId;
    std::optional<std::int64_t> windowsQpcFrequency;
    std::optional<std::int64_t> linuxThreadId;
    std::optional<int> linuxSchedulerPolicy;
    std::optional<std::int64_t> linuxTimerSlackNs;
    std::optional<int> linuxTimerSlackError;
};

class MediaNodeWaitObservationScope final {
public:
    explicit MediaNodeWaitObservationScope(MediaNodeWaitObservation* value) noexcept : m_value(value)
    {
        if (m_value) { *m_value = {}; m_value->entered = mediaSteadyClockNowNs(); }
    }
    ~MediaNodeWaitObservationScope() { if (m_value) m_value->returned = mediaSteadyClockNowNs(); }
    MediaNodeWaitObservationScope(const MediaNodeWaitObservationScope&) = delete;
    MediaNodeWaitObservationScope& operator=(const MediaNodeWaitObservationScope&) = delete;
private:
    MediaNodeWaitObservation* m_value;
};
} // namespace media::ffmpeg::graph
