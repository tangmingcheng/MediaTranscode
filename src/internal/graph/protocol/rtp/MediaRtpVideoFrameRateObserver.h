#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpVideoFrameRateObserver final {
public:
    static ::media::Result<MediaRtpVideoFrameRateObserver> create(
        int clockRate);

    ::media::Status observe(const MediaRtpPacket& packet);
    void discontinuity() noexcept;
    std::optional<MediaRational> frameRate() const noexcept;

private:
    explicit MediaRtpVideoFrameRateObserver(int clockRate) noexcept;

    int m_clockRate = 0;
    std::optional<std::uint32_t> m_ssrc;
    std::optional<std::uint32_t> m_completedAccessUnitTimestamp;
    std::optional<MediaRational> m_frameRate;
};

} // namespace media::ffmpeg::graph
