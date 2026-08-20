#include "internal/graph/protocol/rtp/MediaRtpVideoFrameRateObserver.h"

#include <limits>
#include <numeric>

namespace media::ffmpeg::graph {

MediaRtpVideoFrameRateObserver::MediaRtpVideoFrameRateObserver(
    int clockRate) noexcept
    : m_clockRate(clockRate)
{
}

::media::Result<MediaRtpVideoFrameRateObserver>
MediaRtpVideoFrameRateObserver::create(int clockRate)
{
    if (clockRate <= 0) {
        return ::media::Result<MediaRtpVideoFrameRateObserver>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP video frame-rate observation requires a positive clock rate"));
    }
    return ::media::Result<MediaRtpVideoFrameRateObserver>::success(
        MediaRtpVideoFrameRateObserver(clockRate));
}

::media::Status MediaRtpVideoFrameRateObserver::observe(
    const MediaRtpPacket& packet)
{
    if (!m_ssrc || *m_ssrc != packet.ssrc) {
        discontinuity();
        m_ssrc = packet.ssrc;
    }
    if (!packet.marker) {
        return ::media::Status::success();
    }
    if (!m_completedAccessUnitTimestamp) {
        m_completedAccessUnitTimestamp = packet.timestamp;
        return ::media::Status::success();
    }
    const std::uint32_t delta =
        packet.timestamp - *m_completedAccessUnitTimestamp;
    m_completedAccessUnitTimestamp = packet.timestamp;
    if (delta == 0) {
        return ::media::Status::success();
    }
    const std::uint32_t divisor = std::gcd(
        static_cast<std::uint32_t>(m_clockRate), delta);
    const std::uint32_t numerator =
        static_cast<std::uint32_t>(m_clockRate) / divisor;
    const std::uint32_t denominator = delta / divisor;
    if (numerator > static_cast<std::uint32_t>(
            (std::numeric_limits<int>::max)()) ||
        denominator > static_cast<std::uint32_t>(
            (std::numeric_limits<int>::max)())) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "observed RTP video frame rate is not representable"));
    }
    m_frameRate = MediaRational{
        static_cast<int>(numerator), static_cast<int>(denominator)};
    return ::media::Status::success();
}

void MediaRtpVideoFrameRateObserver::discontinuity() noexcept
{
    m_ssrc.reset();
    m_completedAccessUnitTimestamp.reset();
    m_frameRate.reset();
}

std::optional<MediaRational>
MediaRtpVideoFrameRateObserver::frameRate() const noexcept
{
    return m_frameRate;
}

} // namespace media::ffmpeg::graph
