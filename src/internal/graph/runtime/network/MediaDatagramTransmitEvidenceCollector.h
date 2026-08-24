#pragma once

#include "internal/graph/runtime/network/MediaDatagramTransmitPort.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDatagramTransmitEvidenceTelemetry final {
    std::uint64_t submitted = 0;
    std::uint64_t observed = 0;
    std::uint64_t late = 0;
    std::uint64_t lost = 0;
    std::uint64_t duplicate = 0;
    std::uint64_t crossGeneration = 0;
    std::uint64_t unmatched = 0;
    bool transmitTimestampCoverageComplete = false;
    bool deliveryEvidenceProven = false;
};

class MediaDatagramTransmitEvidenceCollector final {
public:
    static ::media::Result<MediaDatagramTransmitEvidenceCollector> create(
        std::uint64_t generation,
        std::optional<MediaDatagramTransmitEvidencePlan> plan,
        std::vector<MediaDatagramTransmitPort*> ports);

    ::media::Status recordSubmitted(
        std::uint64_t endpointId,
        std::uint64_t evidenceId,
        MediaRunningTime submittedAt) noexcept;
    ::media::Status drainAvailable(MediaRunningTime now) noexcept;
    const MediaDatagramTransmitEvidenceTelemetry& telemetry() const noexcept
    {
        return m_telemetry;
    }

private:
    struct Pending final {
        std::uint64_t endpointId;
        MediaRunningTime submittedAt;
    };

    MediaDatagramTransmitEvidenceCollector(
        std::uint64_t generation,
        std::optional<MediaDatagramTransmitEvidencePlan> plan,
        std::vector<MediaDatagramTransmitPort*> ports) noexcept;

    ::media::Status coverageFailure(const char* message) noexcept;

    std::uint64_t m_generation;
    std::optional<MediaDatagramTransmitEvidencePlan> m_plan;
    std::vector<MediaDatagramTransmitPort*> m_ports;
    std::unordered_map<std::uint64_t, Pending> m_pending;
    std::vector<std::uint64_t> m_recentObserved;
    std::optional<std::uint64_t> m_lastSubmittedEvidenceId;
    MediaDatagramTransmitEvidenceTelemetry m_telemetry;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
