#pragma once

#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaMpegTsTimingConstraintSource : std::uint8_t {
    Unknown = 0,
    ProtocolMaximum = 1,
    ReceiverCapability = 2,
    PreparedCadence = 3,
    DeploymentServiceSlo = 4,
    PlannerDerivedMinimum = 5
};

struct MediaMpegTsTimingConstraint final {
    MediaRunningTime value = MediaRunningTime::fromNanoseconds(0);
    std::string authority;
    MediaMpegTsTimingConstraintSource source =
        MediaMpegTsTimingConstraintSource::Unknown;
    friend bool operator==(const MediaMpegTsTimingConstraint&,
                           const MediaMpegTsTimingConstraint&) = default;
};

class MediaMpegTsTimingPolicy final {
public:
    static ::media::Result<MediaMpegTsTimingPolicy> create(
        MediaMpegTsTimingConstraint pcrInterval,
        MediaMpegTsTimingConstraint maximumPcrGap,
        MediaMpegTsTimingConstraint psiRepeatInterval,
        MediaMpegTsTimingConstraint maximumReleaseJitter,
        int timestampTimeBaseNumerator,
        int timestampTimeBaseDenominator);

    const MediaMpegTsTimingConstraint& pcrInterval() const noexcept;
    const MediaMpegTsTimingConstraint& maximumPcrGap() const noexcept;
    const MediaMpegTsTimingConstraint& psiRepeatInterval() const noexcept;
    const MediaMpegTsTimingConstraint& maximumReleaseJitter() const noexcept;
    MediaTsOutputClockPolicy clockPolicy() const noexcept;
    int timestampTimeBaseNumerator() const noexcept;
    int timestampTimeBaseDenominator() const noexcept;

    friend bool operator==(const MediaMpegTsTimingPolicy&,
                           const MediaMpegTsTimingPolicy&) = default;

private:
    MediaMpegTsTimingPolicy(
        MediaMpegTsTimingConstraint pcrInterval,
        MediaMpegTsTimingConstraint maximumPcrGap,
        MediaMpegTsTimingConstraint psiRepeatInterval,
        MediaMpegTsTimingConstraint maximumReleaseJitter,
        int timestampTimeBaseNumerator,
        int timestampTimeBaseDenominator) noexcept;

    MediaMpegTsTimingConstraint m_pcrInterval;
    MediaMpegTsTimingConstraint m_maximumPcrGap;
    MediaMpegTsTimingConstraint m_psiRepeatInterval;
    MediaMpegTsTimingConstraint m_maximumReleaseJitter;
    int m_timestampTimeBaseNumerator;
    int m_timestampTimeBaseDenominator;
};

} // namespace media::ffmpeg::graph
