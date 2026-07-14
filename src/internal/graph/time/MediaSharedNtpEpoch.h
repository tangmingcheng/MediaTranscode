#pragma once

#include "internal/graph/time/MediaNtpTimestamp.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <chrono>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaSharedNtpEpoch final {
public:
    static ::media::Result<MediaSharedNtpEpoch> create(
        MediaRunningTime masterAtCapture,
        std::chrono::nanoseconds unixTimeAtCapture) noexcept;

    ::media::Result<MediaNtpTimestamp> map(
        MediaRunningTime masterTime) const noexcept;

    MediaRunningTime masterAtCapture() const noexcept
    {
        return m_masterAtCapture;
    }

    std::chrono::nanoseconds unixTimeAtCapture() const noexcept
    {
        return std::chrono::nanoseconds(m_unixNanosecondsAtCapture);
    }

private:
    MediaSharedNtpEpoch(MediaRunningTime masterAtCapture,
                        std::int64_t unixNanosecondsAtCapture) noexcept;

    static ::media::Result<MediaNtpTimestamp> fromUnixNanoseconds(
        std::int64_t unixNanoseconds) noexcept;

    MediaRunningTime m_masterAtCapture;
    std::int64_t m_unixNanosecondsAtCapture;
};

} // namespace media::ffmpeg::graph
