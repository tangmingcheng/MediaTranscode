#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaInitialClockAcquisitionDeadline final {
public:
    static ::media::Result<MediaInitialClockAcquisitionDeadline> create(
        MediaRunningTime plannedTimeout);

    ::media::Status establish(MediaRunningTime masterNow);
    ::media::Status preflight(MediaRunningTime masterNow) const;
    const std::optional<MediaRunningTime>& deadline() const noexcept;
    void clear() noexcept;

private:
    explicit MediaInitialClockAcquisitionDeadline(
        MediaRunningTime plannedTimeout) noexcept;

    MediaRunningTime m_plannedTimeout;
    std::optional<MediaRunningTime> m_deadline;
};

} // namespace media::ffmpeg::graph
