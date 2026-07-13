#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/time/MediaTimestampUnwrapper.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaTsProgramClockPolicy final {
    std::uint16_t programNumber = 0;
    std::uint16_t pmtPid = 0;
    std::uint16_t pcrPid = 0;
    std::int64_t pcrInterval27Mhz = 0;
    std::int64_t maximumJitter27Mhz = 0;
    std::int64_t maximumGap27Mhz = 0;
};

struct MediaTsPcrObservation final {
    std::uint64_t byteOffset = 0;
    std::uint16_t programNumber = 0;
    std::uint16_t pmtPid = 0;
    std::uint16_t pcrPid = 0;
    std::uint64_t pcr27Mhz = 0;
    bool discontinuity = false;
};

struct MediaTsPcrCalibration final {
    std::uint64_t generation = 0;
    std::int64_t pcr27Mhz = 0;
    MediaRunningTime sourceTime = MediaRunningTime::fromNanoseconds(0);
};

class MediaTsProgramClockTracker final {
public:
    static ::media::Result<MediaTsProgramClockTracker> create(
        MediaTsProgramClockPolicy policy,
        std::uint64_t generation);

    ::media::Status observe(const MediaTsPcrObservation& observation);
    ::media::Status observeContinuityLoss(std::uint16_t pid);
    ::media::Status observeProgramIdentity(std::uint16_t programNumber,
                                           std::uint16_t pmtPid,
                                           std::uint16_t pcrPid) const;
    bool ready() const noexcept { return m_ready; }
    std::uint64_t generation() const noexcept { return m_generation; }
    ::media::Result<MediaTsPcrCalibration> calibration() const;

private:
    MediaTsProgramClockTracker(MediaTsProgramClockPolicy policy,
                               std::uint64_t generation,
                               MediaTimestampUnwrapper unwrapper) noexcept;
    void reacquire();

    MediaTsProgramClockPolicy m_policy;
    std::uint64_t m_generation;
    MediaTimestampUnwrapper m_unwrapper;
    std::optional<std::int64_t> m_previousPcr;
    std::optional<MediaTsPcrCalibration> m_calibration;
    bool m_ready = false;
};

} // namespace media::ffmpeg::graph
