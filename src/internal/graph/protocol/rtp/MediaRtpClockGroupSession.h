#pragma once

#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpIngressEvent.h"
#include <memory>

namespace media::ffmpeg::graph {

struct MediaRtpClockGroupSessionConfig final {
    MediaRtpSourceClockMapperConfig video;
    std::optional<MediaRtpSourceClockMapperConfig> audio;
    MediaRtpClockGroupValidatorConfig validator;
};

struct MediaRtpClockGroupConsumption final {
    std::int64_t observationTimeNs;
    std::optional<::media::ErrorInfo> rejected;
};

// Single-owner state; the caller retains scheduling and publication boundaries.
class MediaRtpClockGroupSession final {
public:
    static ::media::Result<MediaRtpClockGroupSession> create(MediaRtpClockGroupSessionConfig config);
    MediaRtpClockGroupSession(MediaRtpClockGroupSession&&) noexcept = default;
    MediaRtpClockGroupSession& operator=(MediaRtpClockGroupSession&&) noexcept = default;
    MediaRtpClockGroupSession(const MediaRtpClockGroupSession&) = delete;
    MediaRtpClockGroupSession& operator=(const MediaRtpClockGroupSession&) = delete;
    ::media::Result<MediaRtpClockGroupConsumption> consume(
        const MediaRtpIngressEventView& event, MediaStreamKind streamKind,
        bool selectedClock, std::int64_t nowNs);
    ::media::Result<MediaRtpClockGroupSnapshot> snapshot(std::int64_t observedAtNs,
        const std::optional<MediaRtpClosedClockBoundary>& closed);
    bool hasAudio() const noexcept { return m_audioConfig.has_value(); }
private:
    MediaRtpClockGroupSession(MediaRtpClockGroupSessionConfig config,
        MediaRtpClockGroupValidator validator);
    ::media::Status processClock(const MediaRtpIngressEventView& event, MediaStreamKind streamKind);
    MediaRtpSourceClockMapperConfig m_videoConfig;
    std::optional<MediaRtpSourceClockMapperConfig> m_audioConfig;
    std::unique_ptr<MediaRtpSourceClockMapper> m_videoMapper;
    std::unique_ptr<MediaRtpSourceClockMapper> m_audioMapper;
    std::unique_ptr<MediaRtpClockGroupValidator> m_validator;
    std::optional<std::uint64_t> m_videoGeneration, m_audioGeneration;
    std::optional<std::uint64_t> m_minimumVideoGeneration, m_minimumAudioGeneration;
    std::optional<std::uint64_t> m_videoSequence, m_audioSequence;
};

} // namespace media::ffmpeg::graph
