#pragma once

#include "internal/graph/planner/realtime/MediaRtpInputPlayoutPlan.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/time/MediaTimestampUnwrapper.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpAccessUnitPlayoutBuffer final {
public:
    static ::media::Result<MediaRtpAccessUnitPlayoutBuffer> create(
        MediaRtpInputPlayoutPlan plan,
        int clockRate);

    ::media::Status push(MediaBufferRef accessUnit,
                         std::uint32_t rtpTimestamp);
    ::media::Result<std::optional<MediaBufferRef>> popReady();
    std::optional<MediaRunningTime> nextReadyAt() const noexcept;
    void reset() noexcept;

private:
    struct Entry final {
        MediaBufferRef accessUnit;
        std::int64_t mediaTimeNanoseconds;
        MediaRunningTime readyAt;
        std::uint64_t payloadBytes;
    };

    MediaRtpAccessUnitPlayoutBuffer(MediaRtpInputPlayoutPlan plan,
                                    int clockRate,
                                    MediaTimestampUnwrapper unwrapper) noexcept;
    ::media::Result<std::int64_t> unwrapMediaTime(
        std::uint32_t rtpTimestamp);
    ::media::Result<MediaRunningTime> calculateReadyAt(
        MediaRunningTime mediaTime) const;
    ::media::Status activateIfReady(MediaRunningTime observedAt);

    MediaRtpInputPlayoutPlan m_plan;
    int m_clockRate = 0;
    MediaTimestampUnwrapper m_unwrapper;
    std::deque<Entry> m_entries;
    std::uint64_t m_retainedPayloadBytes = 0;
    std::uint64_t m_generation = 1;
    std::optional<MediaRunningTime> m_firstMediaTime;
    std::optional<MediaRunningTime> m_firstReadyAt;
};

} // namespace media::ffmpeg::graph
