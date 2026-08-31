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
    Unknown = 0,
    NonBlockingAtomicEnqueue = 1
};

enum class MediaDatagramOrderingMode {
    Unknown = 0,
    CanonicalOrdered = 1
};

enum class MediaDatagramLimitFailureMode {
    Unknown = 0,
    Terminate = 1
};

enum class MediaDatagramPersistentStateMode {
    Unknown = 0,
    PreserveScopeDebt = 1
};

enum class MediaDatagramTransmitEvidenceKind {
    Unknown = 0,
    TransmitTimestamp = 1
};

enum class MediaDatagramEvidenceCoverageGapPolicy {
    Unknown = 0,
    Report = 1,
    Fail = 2
};

enum class MediaDatagramServiceScopeKind {
    Unknown = 0,
    ManagedEgress = 1,
    ProvisionedEgress = 2
};

struct MediaDatagramMtuEvidence final {
    std::string authority;
    std::uint64_t maximumIpPacketBytes;
    std::uint64_t ipHeaderBytes;
    std::uint64_t transportHeaderBytes;
    std::uint64_t senderMaximumPayloadBytes;

    friend bool operator==(const MediaDatagramMtuEvidence&,
                           const MediaDatagramMtuEvidence&) = default;
};

struct MediaDatagramServiceScopePlan final {
    MediaDatagramServiceScopeKind kind;
    std::string scopeId;
    std::string coverageAuthority;
    std::vector<std::uint64_t> endpointCoverage;

    friend bool operator==(const MediaDatagramServiceScopePlan&,
                           const MediaDatagramServiceScopePlan&) = default;
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
    std::uint64_t targetEffectiveSendBufferBytes;
    std::uint64_t minimumEffectiveSendBufferBytes;
    std::uint64_t maximumAdmittedEffectiveSendBufferBytes;

    friend bool operator==(const MediaDatagramEndpointPlan&,
                           const MediaDatagramEndpointPlan&) = default;
};

struct MediaDatagramServiceCurvePlan final {
    std::uint64_t pacingWireBytesPerSecond;
    std::uint64_t maximumWireBytesPerSecond;
    std::uint64_t burstWireBytes;
    MediaRunningTime targetResidence;
    MediaRunningTime maximumReleaseJitter;
    std::string authority;

    friend bool operator==(const MediaDatagramServiceCurvePlan&,
                           const MediaDatagramServiceCurvePlan&) = default;
};

struct MediaDatagramPlannedWireCost final {
    std::uint64_t wireBytes;
    MediaRunningTime pacingDebtDuration;

    friend bool operator==(const MediaDatagramPlannedWireCost&,
                           const MediaDatagramPlannedWireCost&) = default;
};

struct MediaDatagramWireDeadlinePlan final {
    std::uint64_t endpointId;
    MediaRunningTime maximumResidence;

    ::media::Result<MediaRunningTime> canonicalDeadline(
        MediaRunningTime canonicalRelease) const noexcept;

    friend bool operator==(const MediaDatagramWireDeadlinePlan&,
                           const MediaDatagramWireDeadlinePlan&) = default;
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

struct MediaDatagramNetworkMemoryPlan final {
    std::uint64_t maximumTotalBytes;
    std::uint64_t reservedUserspaceBytes;
    std::uint64_t maximumSocketBytes;

    friend bool operator==(const MediaDatagramNetworkMemoryPlan&,
                           const MediaDatagramNetworkMemoryPlan&) = default;
};

struct MediaDatagramTransmitEvidencePlan final {
    MediaDatagramTransmitEvidenceKind kind;
    MediaDatagramEvidenceCoverageGapPolicy coverageGapPolicy;
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
    MediaDatagramServiceScopePlan serviceScope;
    std::vector<MediaDatagramEndpointPlan> endpoints;
    MediaDatagramServiceCurvePlan serviceCurve;
    MediaDatagramBacklogPlan backlog;
    MediaDatagramBatchPlan batch;
    MediaDatagramNetworkMemoryPlan networkMemory;
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

    ::media::Result<MediaDatagramShapingPlanEncoding> encode() const noexcept;
    ::media::Result<MediaDatagramShapingPlan> clone() const noexcept;
    const std::string& sessionKey() const noexcept;
    std::uint64_t generation() const noexcept;
    const MediaDatagramServiceScopePlan& serviceScope() const noexcept;
    const std::vector<MediaDatagramEndpointPlan>& endpoints() const noexcept;
    const MediaDatagramEndpointPlan* endpoint(
        std::uint64_t endpointId) const noexcept;
    const MediaDatagramServiceCurvePlan& serviceCurve() const noexcept;
    const MediaDatagramBacklogPlan& backlog() const noexcept;
    const MediaDatagramBatchPlan& batch() const noexcept;
    const MediaDatagramNetworkMemoryPlan& networkMemory() const noexcept;
    MediaDatagramSubmitMode submitMode() const noexcept;
    MediaDatagramOrderingMode orderingMode() const noexcept;
    MediaDatagramLimitFailureMode pressureFailureMode() const noexcept;
    MediaDatagramLimitFailureMode deadlineFailureMode() const noexcept;
    MediaDatagramPersistentStateMode persistentStateMode() const noexcept;
    const std::optional<MediaDatagramTransmitEvidencePlan>&
    evidence() const noexcept;
    ::media::Result<MediaDatagramPlannedWireCost> plannedWireCost(
        std::uint64_t endpointId, std::uint64_t payloadBytes) const;
    ::media::Result<MediaDatagramWireDeadlinePlan> wireDeadlinePlan(
        std::uint64_t endpointId) const noexcept;

private:
    explicit MediaDatagramShapingPlan(
        MediaDatagramShapingPlanEncoding encoding) noexcept;

    MediaDatagramShapingPlanEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
