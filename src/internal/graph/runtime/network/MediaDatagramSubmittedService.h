#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// Confirmed single-datagram submission in the bound session clock domain.
// This records sender service debt, not a shared-scope reservation or delivery.
struct MediaDatagramSubmittedService final {
    std::uint64_t generation;
    std::uint64_t globalSequence;
    MediaRunningTime submittedAt;
    MediaRunningTime serviceDuration;
    // Observed while completing this submission's bound scope permit. Other
    // members may advance the scope immediately afterwards; this is no lease.
    std::optional<MediaRunningTime> scopeAvailableAt;

    friend bool operator==(const MediaDatagramSubmittedService&,
                           const MediaDatagramSubmittedService&) = default;

    ::media::Result<MediaRunningTime> serviceAvailableAt() const noexcept
    {
        if (submittedAt.nanoseconds() < 0 || serviceDuration.nanoseconds() <= 0)
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument("submitted datagram service facts are invalid"));
        return submittedAt.checkedAdd(serviceDuration);
    }
};

} // namespace media::ffmpeg::graph
