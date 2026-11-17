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

struct MediaRtpPreparedClockObservation final {
    MediaRtcpPreparedObservation tracker;
    MediaRtpClockObservationSchedule schedule;
    bool evidencePublished;
    std::optional<MediaRtpClockAgeTransition> age;
    bool retireAssembly;
};

// One serialized source owner orders media, continuity and RTCP observations. This
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
    ::media::Result<MediaRtpPreparedClockObservation> prepareDiscontinuity(std::uint64_t generationBefore) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareRtcp(
        std::span<const std::uint8_t> datagram, std::int64_t observedAtNs, std::int64_t evaluationAtNs) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareEvidence(std::int64_t evaluationAtNs) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareClockTransition(std::int64_t evaluationAtNs) const;
    ::media::Result<MediaRtpPreparedClockObservation> prepareMemberRetirement(
        std::uint64_t expectedIngressGeneration, std::uint32_t expectedSsrc,
        MediaRtpSourceUnavailableReason reason, std::int64_t observedAtNs) const;
    ::media::Status commitClockObservation(MediaRtpPreparedClockObservation&& prepared);
    ::media::Result<int> receiveTimeoutMs(std::int64_t observedAtNs,
        int plannedMaximumMs) const noexcept;
    std::optional<std::int64_t> nextDeadlineNs() const noexcept;
    std::uint64_t generation() const noexcept;
    const MediaRtpInputClockSessionConfig& config() const noexcept;

private:
    ::media::Result<MediaRtpPreparedClockObservation> materializeObservation(MediaRtcpPreparedObservation prepared) const;
    MediaRtpInputClockSession(MediaRtpInputClockSessionConfig config,
        MediaRtpClockObservationSchedule schedule, MediaLineageAllocationScope allocation);
    MediaLineageAllocationScope m_allocation;
    MediaRtpInputClockSessionConfig m_config;
    MediaRtcpSenderReportTracker m_tracker;
    MediaRtpClockObservationSchedule m_schedule;
};

} // namespace media::ffmpeg::graph
