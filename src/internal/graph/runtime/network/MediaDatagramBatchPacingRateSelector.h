#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <span>

namespace media::ffmpeg::graph {

struct MediaDatagramPacingReservationFact final {
    std::uint64_t wireBytes;
    MediaRunningTime canonicalRelease;
    MediaRunningTime targetCompletion;
    MediaRunningTime maximumCompletion;
    MediaRunningTime sustainedDebtDuration;
};

struct MediaDatagramBatchPacingRateSelection final {
    std::uint64_t wireBytesPerSecond;
    bool targetResidenceSatisfied;
};

class MediaDatagramBatchPacingRateSelector final {
public:
    static ::media::Result<MediaDatagramBatchPacingRateSelection>
    selectMinimumFeasibleRate(
        std::span<const MediaDatagramPacingReservationFact> reservations,
        MediaRunningTime now,
        std::optional<MediaRunningTime> physicalAvailable,
        std::optional<MediaRunningTime> sustainedDebtUntil,
        MediaRunningTime burstDebtDuration,
        std::uint64_t sustainedWireBytesPerSecond,
        std::uint64_t peakWireBytesPerSecond);
};

} // namespace media::ffmpeg::graph
