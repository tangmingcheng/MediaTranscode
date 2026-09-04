#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaDatagramPacingContract final {
    std::string sessionKey;
    std::string serviceScopeId;
    std::uint64_t generation;
    std::uint64_t wireBytesPerSecond;
    std::uint64_t maximumWireBytesPerSecond;
    MediaRunningTime queueTimeLimit;

    friend bool operator==(const MediaDatagramPacingContract&,
                           const MediaDatagramPacingContract&) = default;
};

struct MediaDatagramPacingQueueState final {
    std::uint64_t wireBytes;
    MediaRunningTime averageResidence;

    friend bool operator==(const MediaDatagramPacingQueueState&,
                           const MediaDatagramPacingQueueState&) = default;
};

struct MediaDatagramPacingJob final {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::uint64_t globalSequence;
    std::uint64_t wireBytes;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    MediaDatagramPacingQueueState queue;
};

struct MediaDatagramPacingReservation final {
    std::uint64_t globalSequence;
    MediaRunningTime notBefore;
    MediaRunningTime notAfter;
    MediaRunningTime serviceDuration;
    std::uint64_t wireBytesPerSecond;
};

struct MediaDatagramPacingTelemetry final {
    std::uint64_t reservedDatagrams = 0;
    std::uint64_t submittedDatagrams = 0;
    std::int64_t maximumSubmitLatenessNanoseconds = 0;
    std::uint64_t worstLateGlobalSequence = 0;
    std::uint64_t rateAdaptations = 0;
    std::uint64_t capacityLimitedReservations = 0;
    std::uint64_t maximumWireBytesPerSecond = 0;
    bool counterSaturated = false;
};

class MediaDatagramPacingController final {
public:
    static ::media::Result<std::unique_ptr<MediaDatagramPacingController>>
    create(MediaDatagramPacingContract contract);

    ::media::Status rebind(MediaDatagramPacingContract contract);
    ::media::Result<MediaDatagramPacingReservation> reserve(
        const MediaDatagramPacingJob& job,
        MediaRunningTime now);
    ::media::Status markSubmitted(
        std::uint64_t globalSequence,
        MediaRunningTime submitStartedAt,
        MediaRunningTime submitCompletedAt) noexcept;

    const MediaDatagramPacingContract& contract() const noexcept
    {
        return m_contract;
    }
    const MediaDatagramPacingTelemetry& telemetry() const noexcept
    {
        return m_telemetry;
    }

private:
    struct PendingReservation final {
        MediaDatagramPacingReservation value;
    };

    explicit MediaDatagramPacingController(
        MediaDatagramPacingContract contract) noexcept;

    MediaDatagramPacingContract m_contract;
    MediaDatagramPacingTelemetry m_telemetry;
    std::optional<PendingReservation> m_pending;
    std::optional<MediaRunningTime> m_theoreticalArrivalTime;
    std::optional<MediaRunningTime> m_lastObservedTime;
    std::optional<std::uint64_t> m_lastSubmittedSequence;
};

} // namespace media::ffmpeg::graph
