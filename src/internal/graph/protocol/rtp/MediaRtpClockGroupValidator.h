#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupPolicy.h"
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
    std::int64_t maximumInterStreamClockOffsetSkewNs;
    std::int64_t videoCnameTimeoutNs;
    std::int64_t audioCnameTimeoutNs;
    MediaRtpCommonEpochPolicy commonEpochPolicy;
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
    enum class Phase {
        InitialAcquisition,
        ActiveGeneration
    };

    struct StreamState final {
        MediaRtcpClockEvidence evidence;
        MediaRtpSourceClockCalibration calibration;
    };

    explicit MediaRtpClockGroupValidator(MediaRtpClockGroupValidatorConfig config) noexcept;
    void discardExpiredInitialCandidates(std::int64_t observedAtNs) noexcept;
    bool initialCandidateIsFresh(
        const StreamState& stream,
        std::int64_t observedAtNs,
        std::int64_t cnameTimeoutNs) const noexcept;
    void clear(bool requireReacquisition) noexcept;

    MediaRtpClockGroupValidatorConfig m_config;
    std::optional<StreamState> m_video;
    std::optional<StreamState> m_audio;
    std::optional<MediaRunningTime> m_commonSourceEpoch;
    std::uint64_t m_groupGeneration = 0;
    bool m_reacquireRequired = false;
    Phase m_phase = Phase::InitialAcquisition;
};

} // namespace media::ffmpeg::graph
