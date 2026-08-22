#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaDatagramSubmitMode {
    NonBlockingAtomicEnqueue
};

enum class MediaDatagramOrderingMode {
    CanonicalOrdered
};

enum class MediaDatagramLimitFailureMode {
    Terminate
};

enum class MediaDatagramPersistentStateMode {
    PreserveScopeDebt
};

enum class MediaDatagramTransmitEvidenceKind {
    TransmitTimestamp,
    ZeroCopyCompletion,
    TransmitTimestampAndZeroCopyCompletion
};

struct MediaDatagramMtuEvidence final {
    std::string authority;
    std::uint64_t maximumIpPacketBytes;
    std::uint64_t ipHeaderBytes;
    std::uint64_t udpHeaderBytes;
    std::uint64_t senderMaximumPayloadBytes;

    friend bool operator==(const MediaDatagramMtuEvidence&,
                           const MediaDatagramMtuEvidence&) = default;
};

struct MediaDatagramEndpointPlan final {
    std::uint64_t endpointId;
    MediaIpAddressFamily addressFamily;
    std::string numericAddress;
    std::uint16_t port;
    MediaDatagramMtuEvidence mtuEvidence;
    std::uint64_t maximumDatagramBytes;
    std::uint64_t maximumPendingDatagrams;
    std::uint64_t maximumPendingBytes;
    MediaRunningTime maximumResidence;
    std::uint64_t socketHardBoundBytes;

    friend bool operator==(const MediaDatagramEndpointPlan&,
                           const MediaDatagramEndpointPlan&) = default;
};

struct MediaDatagramServiceCurvePlan final {
    std::uint64_t serviceBytesPerSecond;
    std::uint64_t peakBytesPerSecond;
    std::uint64_t burstBytes;
    std::string authority;

    friend bool operator==(const MediaDatagramServiceCurvePlan&,
                           const MediaDatagramServiceCurvePlan&) = default;
};

struct MediaDatagramBacklogPlan final {
    std::uint64_t maximumDatagrams;
    std::uint64_t maximumBytes;
    MediaRunningTime maximumResidence;

    friend bool operator==(const MediaDatagramBacklogPlan&,
                           const MediaDatagramBacklogPlan&) = default;
};

struct MediaDatagramBatchPlan final {
    std::uint64_t maximumDatagrams;
    std::uint64_t maximumBytes;

    friend bool operator==(const MediaDatagramBatchPlan&,
                           const MediaDatagramBatchPlan&) = default;
};

struct MediaDatagramTransmitEvidencePlan final {
    MediaDatagramTransmitEvidenceKind kind;
    std::string authority;
    std::uint64_t firstEvidenceId;
    std::uint64_t lastEvidenceId;
    std::uint64_t maximumCorrelationEntries;
    MediaRunningTime maximumDrainResidence;

    friend bool operator==(const MediaDatagramTransmitEvidencePlan&,
                           const MediaDatagramTransmitEvidencePlan&) = default;
};

struct MediaDatagramShapingPlanEncoding final {
    std::string sessionKey;
    std::uint64_t generation;
    std::string serviceScopeId;
    std::vector<MediaDatagramEndpointPlan> endpoints;
    MediaDatagramServiceCurvePlan serviceCurve;
    MediaDatagramBacklogPlan backlog;
    MediaDatagramBatchPlan batch;
    MediaDatagramSubmitMode submitMode;
    MediaDatagramOrderingMode orderingMode;
    MediaDatagramLimitFailureMode pressureFailureMode;
    MediaDatagramLimitFailureMode deadlineFailureMode;
    MediaDatagramPersistentStateMode persistentStateMode;
    std::optional<MediaDatagramTransmitEvidencePlan> evidence;

    friend bool operator==(const MediaDatagramShapingPlanEncoding&,
                           const MediaDatagramShapingPlanEncoding&) = default;
};

class MediaDatagramShapingPlan final {
public:
    static ::media::Result<MediaDatagramShapingPlan> decode(
        MediaDatagramShapingPlanEncoding encoding);

    MediaDatagramShapingPlan(MediaDatagramShapingPlan&&) noexcept = default;
    MediaDatagramShapingPlan& operator=(
        MediaDatagramShapingPlan&&) noexcept = default;
    MediaDatagramShapingPlan(const MediaDatagramShapingPlan&) = delete;
    MediaDatagramShapingPlan& operator=(
        const MediaDatagramShapingPlan&) = delete;

    MediaDatagramShapingPlanEncoding encode() const;
    ::media::Result<MediaDatagramShapingPlan> clone() const;
    const std::string& sessionKey() const noexcept;
    std::uint64_t generation() const noexcept;
    const std::string& serviceScopeId() const noexcept;
    const std::vector<MediaDatagramEndpointPlan>& endpoints() const noexcept;
    const MediaDatagramEndpointPlan* endpoint(
        std::uint64_t endpointId) const noexcept;
    const MediaDatagramServiceCurvePlan& serviceCurve() const noexcept;
    const MediaDatagramBacklogPlan& backlog() const noexcept;
    const MediaDatagramBatchPlan& batch() const noexcept;
    MediaDatagramSubmitMode submitMode() const noexcept;
    MediaDatagramOrderingMode orderingMode() const noexcept;
    MediaDatagramLimitFailureMode pressureFailureMode() const noexcept;
    MediaDatagramLimitFailureMode deadlineFailureMode() const noexcept;
    MediaDatagramPersistentStateMode persistentStateMode() const noexcept;
    const std::optional<MediaDatagramTransmitEvidencePlan>&
    evidence() const noexcept;
    ::media::Status validateDatagram(
        std::uint64_t endpointId, std::uint64_t payloadBytes) const;

private:
    explicit MediaDatagramShapingPlan(
        MediaDatagramShapingPlanEncoding encoding) noexcept;

    MediaDatagramShapingPlanEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
