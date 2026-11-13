#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"

#include <cstdint>
#include <optional>
#include <span>

namespace media::ffmpeg::graph {

struct MediaRtpInputClockSessionConfig final {
    MediaRtcpSenderReportTrackerConfig tracker;
    std::int64_t maximumExtrapolationNs;
    MediaRtpClockLossPolicy lossPolicy;
    MediaRtcpCompositionMode compositionMode;
};

struct MediaRtpClockSessionTransition final {
    MediaRtpClockAgeTransition age;
    std::optional<MediaRtpClockInvalidation> invalidation;
};

struct MediaRtpPreparedRtcpObservation final {
    MediaRtcpPreparedObservation tracker;
    MediaRtpClockObservationSchedule schedule;
    bool evidencePublished;
};

// One receiver owns and orders media, continuity and RTCP observations. This
// component retains the existing tracker/schedule semantics; event delivery,
// packet retirement and recovery gating remain with the input node.
class MediaRtpInputClockSession final {
public:
    static ::media::Result<MediaRtpInputClockSession> create(
        MediaRtpInputClockSessionConfig config, MediaLineageAllocationScope allocation);
    MediaRtpInputClockSession(const MediaRtpInputClockSession&) = delete;
    MediaRtpInputClockSession& operator=(const MediaRtpInputClockSession&) = delete;
    MediaRtpInputClockSession(MediaRtpInputClockSession&&) = default;
    MediaRtpInputClockSession& operator=(MediaRtpInputClockSession&&) = default;

    ::media::Result<std::uint64_t> observeMedia(std::uint32_t ssrc);
    ::media::Status observeDiscontinuity(std::uint64_t generationBefore);
    ::media::Result<MediaRtpPreparedRtcpObservation> prepareRtcp(
        std::span<const std::uint8_t> datagram, std::int64_t observedAtNs, std::int64_t evaluationAtNs) const;
    ::media::Status commitRtcp(MediaRtpPreparedRtcpObservation&& prepared);
    ::media::Result<std::optional<MediaRtcpClockEvidence>> takeEvidenceUpdate(
        std::int64_t observedAtNs);
    ::media::Result<int> receiveTimeoutMs(std::int64_t observedAtNs,
        int plannedMaximumMs) const noexcept;
    ::media::Result<std::optional<MediaRtpClockSessionTransition>> transition(
        std::int64_t observedAtNs);
    std::uint64_t generation() const noexcept;
    const MediaRtpInputClockSessionConfig& config() const noexcept;

private:
    MediaRtpInputClockSession(MediaRtpInputClockSessionConfig config,
        MediaRtpClockObservationSchedule schedule, MediaLineageAllocationScope allocation);
    MediaLineageAllocationScope m_allocation;
    MediaRtpInputClockSessionConfig m_config;
    MediaRtcpSenderReportTracker m_tracker;
    MediaRtpClockObservationSchedule m_schedule;
};

} // namespace media::ffmpeg::graph
