#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRtpClockGroupState {
    Acquiring,
    Locked,
    Degraded,
    ReacquireRequired
};

struct MediaRtpClockGroupValidatorConfig final {
    std::int64_t senderReportTimeoutNs;
    std::int64_t maximumExtrapolationNs;
    std::int64_t maximumInterStreamSkewNs;
    std::int64_t videoCnameTimeoutNs;
    std::int64_t audioCnameTimeoutNs;
};

struct MediaRtpLockedClockGroup final {
    MediaRunningTime commonSourceEpoch;
    std::vector<std::uint8_t> cname;
    MediaRtpSourceClockCalibration video;
    MediaRtpSourceClockCalibration audio;
};

struct MediaRtpClockGroupSnapshot final {
    MediaRtpClockGroupState state;
    std::uint64_t groupGeneration;
    std::optional<MediaRtpLockedClockGroup> locked;
};

class MediaRtpClockGroupValidator final {
public:
    static ::media::Result<MediaRtpClockGroupValidator> create(
        MediaRtpClockGroupValidatorConfig config);

    ::media::Status observe(MediaStreamKind streamKind,
                            const MediaRtcpClockEvidence& evidence,
                            MediaRtpSourceClockCalibration calibration);
    MediaRtpClockGroupSnapshot snapshot(std::int64_t observedAtNs);
    void invalidate() noexcept;

private:
    struct StreamState final {
        MediaRtcpClockEvidence evidence;
        MediaRtpSourceClockCalibration calibration;
    };

    explicit MediaRtpClockGroupValidator(MediaRtpClockGroupValidatorConfig config) noexcept;
    void clear(bool requireReacquisition) noexcept;

    MediaRtpClockGroupValidatorConfig m_config;
    std::optional<StreamState> m_video;
    std::optional<StreamState> m_audio;
    std::optional<MediaRunningTime> m_commonSourceEpoch;
    std::uint64_t m_groupGeneration = 0;
    bool m_reacquireRequired = false;
    bool m_everLocked = false;
};

} // namespace media::ffmpeg::graph
