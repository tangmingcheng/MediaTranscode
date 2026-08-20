#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaForwardOnlyDatagramPacer final {
public:
    ::media::Result<MediaRunningTime> prepare(
        MediaRunningTime plannedEligibility,
        MediaRunningTime submissionDeadline,
        MediaRunningTime serviceDuration);
    ::media::Status commitSuccessfulSubmit(
        MediaRunningTime actualPreSubmit) noexcept;
    void reset() noexcept;

private:
    struct Pending final {
        MediaRunningTime eligibility;
        MediaRunningTime deadline;
        MediaRunningTime serviceDuration;
    };

    std::optional<MediaRunningTime> m_previousPreSubmit;
    std::optional<MediaRunningTime> m_previousServiceDuration;
    std::optional<Pending> m_pending;
};

} // namespace media::ffmpeg::graph
