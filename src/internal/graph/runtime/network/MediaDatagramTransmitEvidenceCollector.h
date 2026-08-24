#pragma once

#include "internal/graph/runtime/network/MediaDatagramTransmitPort.h"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDatagramTransmitEvidenceTelemetry final {
    std::uint64_t submitted = 0;
    std::uint64_t timestampTracked = 0;
    std::uint64_t timestampUntracked = 0;
    std::uint64_t observed = 0;
    std::uint64_t late = 0;
    std::uint64_t lost = 0;
    std::uint64_t duplicate = 0;
    std::uint64_t crossGeneration = 0;
    std::uint64_t unmatched = 0;
    std::uint64_t txTimeMissed = 0;
    std::uint64_t txTimeInvalid = 0;
    MediaDatagramTransmitTimestampSource lastTimestampSource =
        MediaDatagramTransmitTimestampSource::Unknown;
    std::uint64_t lastRawTimestampCounter = 0;
    std::uint64_t lastRawTimestampFrequency = 0;
    bool transmitTimestampCoverageComplete = false;
    bool deliveryEvidenceProven = false;
    bool counterSaturated = false;
};

struct MediaDatagramTransmitEvidenceReservation final {
    std::uint64_t evidenceId;
    std::optional<std::uint32_t> platformCorrelationId;
};

struct MediaDatagramTransmitEvidenceEndpoint final {
    std::uint64_t endpointId;
    MediaDatagramTransmitPort* port;
    MediaDatagramTransmitPortCapabilities capabilities;
};

class MediaDatagramTransmitEvidenceCollector final {
public:
    static ::media::Result<MediaDatagramTransmitEvidenceCollector> create(
        std::uint64_t generation,
        std::uint64_t maximumTrackedDatagrams,
        std::optional<MediaDatagramTransmitEvidencePlan> plan,
        std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule,
        std::vector<MediaDatagramTransmitEvidenceEndpoint> endpoints);

    ::media::Result<std::vector<MediaDatagramTransmitEvidenceReservation>>
    reserveBeforeSubmit(
        std::uint64_t endpointId,
        std::span<const std::uint64_t> evidenceIds,
        std::span<const std::optional<std::uint64_t>> launchTimes,
        MediaRunningTime submittedAt) noexcept;
    void markSubmittedPrefix(
        std::span<const MediaDatagramTransmitEvidenceReservation> reservations,
        std::uint64_t submittedPrefix) noexcept;
    void cancelPrepared(
        std::span<const MediaDatagramTransmitEvidenceReservation> reservations,
        std::uint64_t firstIndex) noexcept;
    ::media::Status drainAvailable(MediaRunningTime now) noexcept;
    ::media::Status settleOnClose(MediaRunningTime now) noexcept;

    const MediaDatagramTransmitEvidenceTelemetry& telemetry() const noexcept
    {
        return m_telemetry;
    }

private:
    enum class EntryState { Prepared, Submitted };

    struct Entry final {
        std::uint64_t endpointId;
        std::uint64_t evidenceId;
        std::uint32_t platformCorrelationId;
        std::optional<std::uint32_t> launchTimeLowBits;
        MediaRunningTime submittedAt;
        EntryState state;
        bool timestampExpected;
        bool timestampObserved;
    };

    struct EndpointState final {
        MediaDatagramTransmitPort* port;
        MediaDatagramTransmitPortCapabilities capabilities;
        std::uint32_t nextKernelCorrelationId = 0;
        bool kernelCorrelationIdsExhausted = false;
        std::unordered_map<std::uint32_t, std::uint64_t> byPlatformId;
        std::unordered_map<std::uint32_t, std::uint64_t> byLaunchTimeLowBits;
    };

    MediaDatagramTransmitEvidenceCollector(
        std::uint64_t generation,
        std::uint64_t maximumTrackedDatagrams,
        std::optional<MediaDatagramTransmitEvidencePlan> plan,
        std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule) noexcept;

    ::media::Status coverageFailure(const char* message) noexcept;
    ::media::Status ingestEvent(
        const MediaDatagramTransmitPlatformEvent& event,
        MediaRunningTime now) noexcept;
    void eraseEntry(std::uint64_t evidenceId) noexcept;
    void incrementCounter(std::uint64_t& counter) noexcept;

    std::uint64_t m_generation;
    std::uint64_t m_maximumTrackedDatagrams;
    std::optional<MediaDatagramTransmitEvidencePlan> m_plan;
    std::optional<MediaDatagramTransmitKernelSchedulePlan> m_kernelSchedule;
    std::unordered_map<std::uint64_t, EndpointState> m_endpoints;
    std::unordered_map<std::uint64_t, Entry> m_entries;
    std::optional<std::uint64_t> m_lastEvidenceId;
    std::optional<MediaRunningTime> m_lastNow;
    MediaDatagramTransmitEvidenceTelemetry m_telemetry;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
