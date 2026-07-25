#pragma once

#include "internal/graph/protocol/rtp/MediaRtpTimestamp.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtpOutputClockMapper final {
public:
    static ::media::Result<MediaRtpOutputClockMapper> create(
        int clockRate,
        std::uint32_t baseTimestamp,
        MediaRunningTime masterOrigin) noexcept;

    ::media::Result<MediaRtpTimestamp> map(
        MediaRunningTime presentationOnMaster) const noexcept;

    int clockRate() const noexcept { return m_clockRate; }
    std::uint32_t baseTimestamp() const noexcept { return m_baseTimestamp; }
    MediaRunningTime masterOrigin() const noexcept { return m_masterOrigin; }

private:
    MediaRtpOutputClockMapper(int clockRate,
                              std::uint32_t baseTimestamp,
                              MediaRunningTime masterOrigin) noexcept;

    int m_clockRate;
    std::uint32_t m_baseTimestamp;
    MediaRunningTime m_masterOrigin;
};

} // namespace media::ffmpeg::graph
