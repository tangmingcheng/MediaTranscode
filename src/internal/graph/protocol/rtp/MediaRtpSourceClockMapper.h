#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/time/MediaTimestampUnwrapper.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRtpSourceClockConfidence {
    Locked,
    Degraded
};

struct MediaRtpSourceClockMapperConfig final {
    int clockRate;
    bool requireCname;
    std::int64_t senderReportTimeoutNs;
    std::int64_t maximumExtrapolationNs;
    std::int64_t maximumResidualNs;
    int maximumRateErrorPpm;
};

struct MediaRtpSourceClockCalibration final {
    std::uint32_t ssrc;
    std::vector<std::uint8_t> cname;
    std::uint64_t generation;
    std::uint32_t rtpAnchor;
    std::int64_t extendedRtpAnchor;
    MediaRunningTime actualSenderReportSourceTime;
    MediaRunningTime continuousSourceAnchor;
    std::int64_t rateNumeratorNs;
    std::int64_t rateDenominatorTicks;
    std::int64_t senderReportObservedAtNs;
    MediaRtpSourceClockConfidence confidence;
};

struct MediaRtpMappedSourceTime final {
    MediaRunningTime sourceTime;
    std::uint64_t generation;
    MediaRtpSourceClockConfidence confidence;
};

class MediaRtpSourceClockMapper final {
public:
    static ::media::Result<MediaRtpSourceClockMapper> create(
        MediaRtpSourceClockMapperConfig config,
        std::uint64_t generation);

    ::media::Status observeSenderReport(const MediaRtcpClockEvidence& evidence);
    ::media::Result<MediaRtpMappedSourceTime> map(std::uint32_t rtpTimestamp,
                                                  std::int64_t observedAtNs);
    ::media::Result<MediaRtpSourceClockCalibration> calibration(
        std::int64_t observedAtNs) const;
    void reset(std::uint64_t generation) noexcept;

private:
    MediaRtpSourceClockMapper(MediaRtpSourceClockMapperConfig config,
                              std::uint64_t generation,
                              MediaTimestampUnwrapper senderReportUnwrapper,
                              MediaTimestampUnwrapper packetUnwrapper) noexcept;

    ::media::Result<MediaRtpSourceClockConfidence> confidence(
        std::int64_t observedAtNs) const;
    ::media::Result<MediaRunningTime> mapExtended(std::int64_t extendedTimestamp) const;
    void invalidate() noexcept;

    MediaRtpSourceClockMapperConfig m_config;
    std::uint64_t m_generation;
    MediaTimestampUnwrapper m_senderReportUnwrapper;
    MediaTimestampUnwrapper m_packetUnwrapper;
    std::optional<MediaRtpSourceClockCalibration> m_calibration;
    std::optional<MediaRtcpNtpTimestamp> m_lastNtp;
    std::optional<std::int64_t> m_packetWrapAdjustment;
    std::optional<std::int64_t> m_estimatorOriginRtp;
    std::optional<MediaRunningTime> m_estimatorOriginSourceTime;
};

} // namespace media::ffmpeg::graph
