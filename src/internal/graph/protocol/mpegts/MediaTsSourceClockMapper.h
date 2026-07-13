#pragma once

#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaTsMappedSourceTiming final {
    std::optional<MediaRunningTime> presentationTime;
    std::optional<MediaRunningTime> decodeTime;
    std::uint64_t generation = 0;
};

class MediaTsSourceClockMapper final {
public:
    static ::media::Result<MediaTsSourceClockMapper> create(
        std::optional<MediaTsPcrCalibration> calibration);

    ::media::Result<MediaTsMappedSourceTiming> map(
        std::optional<std::uint64_t> pts33,
        std::optional<std::uint64_t> dts33);

private:
    MediaTsSourceClockMapper(MediaTsPcrCalibration calibration,
                             MediaTimestampUnwrapper ptsUnwrapper,
                             MediaTimestampUnwrapper dtsUnwrapper) noexcept;
    ::media::Result<std::optional<MediaRunningTime>> mapOne(
        std::optional<std::uint64_t> raw,
        MediaTimestampUnwrapper& unwrapper);

    MediaTsPcrCalibration m_calibration;
    MediaTimestampUnwrapper m_ptsUnwrapper;
    MediaTimestampUnwrapper m_dtsUnwrapper;
};

} // namespace media::ffmpeg::graph
