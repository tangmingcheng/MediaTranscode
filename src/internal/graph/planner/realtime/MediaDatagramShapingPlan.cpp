#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> checkedWireBytes(
    const MediaDatagramEndpointPlan& endpoint,
    std::uint64_t payloadBytes)
{
    using Result = ::media::Result<std::uint64_t>;
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (payloadBytes > maximum - endpoint.mtuEvidence.ipHeaderBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram wire byte accounting overflowed"));
    }
    const auto ipPacketBytes =
        payloadBytes + endpoint.mtuEvidence.ipHeaderBytes;
    if (ipPacketBytes > maximum -
                            endpoint.mtuEvidence.transportHeaderBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram wire byte accounting overflowed"));
    }
    return Result::success(
        ipPacketBytes + endpoint.mtuEvidence.transportHeaderBytes);
}

::media::Status validateMtuEvidence(const MediaDatagramEndpointPlan& endpoint)
{
    const auto& evidence = endpoint.mtuEvidence;
    if (evidence.authority.empty() || evidence.maximumIpPacketBytes == 0 ||
        evidence.ipHeaderBytes == 0 || evidence.transportHeaderBytes == 0 ||
        evidence.senderMaximumPayloadBytes == 0 ||
        evidence.ipHeaderBytes >= evidence.maximumIpPacketBytes ||
        evidence.transportHeaderBytes >
            evidence.maximumIpPacketBytes - evidence.ipHeaderBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram endpoint requires complete MTU evidence"));
    }
    const auto networkPayloadBytes = evidence.maximumIpPacketBytes -
                                     evidence.ipHeaderBytes -
                                     evidence.transportHeaderBytes;
    const auto derivedMaximum =
        (std::min)(networkPayloadBytes, evidence.senderMaximumPayloadBytes);
    if (endpoint.maximumDatagramBytes == 0 ||
        endpoint.maximumDatagramBytes > derivedMaximum) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram endpoint maximum payload exceeds its MTU evidence"));
    }
    return ::media::Status::success();
}

::media::Status validateEndpoint(const MediaDatagramEndpointPlan& endpoint)
{
    auto address = MediaNumericIpAddress::create(endpoint.addressFamily,
                                                 endpoint.numericAddress);
    auto mtu = validateMtuEvidence(endpoint);
    if (endpoint.endpointId == 0 || endpoint.port == 0 || !address || !mtu ||
        endpoint.maximumPendingDatagrams == 0 ||
        endpoint.maximumPendingBytes < endpoint.maximumDatagramBytes ||
        endpoint.targetEffectiveSendBufferBytes == 0 ||
        endpoint.minimumEffectiveSendBufferBytes <
            endpoint.maximumDatagramBytes ||
        endpoint.targetEffectiveSendBufferBytes <
            endpoint.minimumEffectiveSendBufferBytes ||
        endpoint.maximumAdmittedEffectiveSendBufferBytes <
            endpoint.targetEffectiveSendBufferBytes ||
        endpoint.maximumResidence <= MediaRunningTime::fromNanoseconds(0) ||
        endpoint.maximumAdmittedEffectiveSendBufferBytes <
            endpoint.maximumDatagramBytes) {
        return ::media::Status::failure(
            !address
                ? address.error()
                : (!mtu ? mtu.error()
                        : ::media::ErrorInfo::invalidArgument(
                              "datagram endpoint hard bounds are incomplete")));
    }
    return ::media::Status::success();
}

bool knownScopeKind(MediaDatagramServiceScopeKind kind) noexcept
{
    return kind == MediaDatagramServiceScopeKind::ManagedEgress ||
           kind == MediaDatagramServiceScopeKind::ProvisionedEgress;
}

::media::Status
validateScopeCoverage(const MediaDatagramServiceScopePlan& scope,
                      const std::unordered_set<std::uint64_t>& endpointIds)
{
    if (!knownScopeKind(scope.kind) || scope.scopeId.empty() ||
        scope.coverageAuthority.empty() ||
        scope.endpointCoverage.size() != endpointIds.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram service scope requires typed authority and exact "
            "endpoint "
            "coverage"));
    }

    std::unordered_set<std::uint64_t> coveredEndpointIds;
    try {
        coveredEndpointIds.reserve(scope.endpointCoverage.size());
        for (const auto endpointId : scope.endpointCoverage) {
            if (endpointId == 0 ||
                !coveredEndpointIds.insert(endpointId).second) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "datagram service scope endpoint coverage must be "
                        "unique"));
            }
        }
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "datagram service scope endpoint coverage"));
    }
    if (coveredEndpointIds != endpointIds) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram service scope must cover exactly the planned endpoints"));
    }
    return ::media::Status::success();
}

::media::Status
validateEvidence(const MediaDatagramShapingPlanEncoding& encoding)
{
    if (!encoding.evidence)
        return ::media::Status::success();
    const auto& evidence = *encoding.evidence;
    const bool knownGapPolicy =
        evidence.coverageGapPolicy ==
            MediaDatagramEvidenceCoverageGapPolicy::Report ||
        evidence.coverageGapPolicy ==
            MediaDatagramEvidenceCoverageGapPolicy::Fail;
    if (evidence.kind != MediaDatagramTransmitEvidenceKind::TransmitTimestamp ||
        !knownGapPolicy || evidence.authority.empty() ||
        evidence.firstEvidenceId == 0 ||
        evidence.lastEvidenceId < evidence.firstEvidenceId ||
        evidence.maximumCorrelationEntries <
            encoding.backlog.maximumDatagrams ||
        evidence.maximumDrainResidence <=
            MediaRunningTime::fromNanoseconds(0) ||
        evidence.maximumDrainResidence > encoding.backlog.maximumResidence ||
        evidence.lastEvidenceId - evidence.firstEvidenceId ==
            (std::numeric_limits<std::uint64_t>::max)() ||
        evidence.lastEvidenceId - evidence.firstEvidenceId + 1 <
            evidence.maximumCorrelationEntries) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "datagram transmit timestamp evidence plan is incomplete"));
    }
    for (const auto& endpoint : encoding.endpoints) {
        if (evidence.maximumCorrelationEntries <
                endpoint.maximumPendingDatagrams ||
            evidence.maximumDrainResidence > endpoint.maximumResidence) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "datagram transmit timestamp evidence does not close endpoint "
                "correlation bounds"));
        }
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaRunningTime>
MediaDatagramWireDeadlinePlan::canonicalDeadline(
    MediaRunningTime canonicalRelease) const noexcept
{
    using Result = ::media::Result<MediaRunningTime>;
    if (endpointId == 0 ||
        canonicalRelease < MediaRunningTime::fromNanoseconds(0) ||
        maximumResidence <= MediaRunningTime::fromNanoseconds(0)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire deadline requires endpoint identity, non-negative release, and positive hard residence"));
    }
    return canonicalRelease.checkedAdd(maximumResidence);
}

::media::Result<MediaDatagramShapingPlan>
MediaDatagramShapingPlan::decode(MediaDatagramShapingPlanEncoding encoding)
{
    using Result = ::media::Result<MediaDatagramShapingPlan>;
    if (encoding.sessionKey.empty() || encoding.generation == 0 ||
        encoding.endpoints.empty() || encoding.serviceCurve.authority.empty() ||
        encoding.serviceCurve.pacingWireBytesPerSecond == 0 ||
        encoding.serviceCurve.burstWireBytes == 0 ||
        encoding.serviceCurve.targetResidence <=
            MediaRunningTime::fromNanoseconds(0) ||
        encoding.serviceCurve.maximumReleaseJitter <=
            MediaRunningTime::fromNanoseconds(0) ||
        encoding.serviceCurve.maximumReleaseJitter >=
            encoding.serviceCurve.targetResidence ||
        encoding.backlog.maximumDatagrams == 0 ||
        encoding.backlog.maximumBytes == 0 ||
        encoding.backlog.maximumResidence <=
            MediaRunningTime::fromNanoseconds(0) ||
        encoding.serviceCurve.targetResidence >
            encoding.backlog.maximumResidence ||
        encoding.batch.maximumDatagrams == 0 ||
        encoding.batch.maximumBytes == 0 ||
        encoding.batch.maximumDatagrams > encoding.backlog.maximumDatagrams ||
        encoding.batch.maximumBytes > encoding.backlog.maximumBytes ||
        encoding.networkMemory.maximumTotalBytes == 0 ||
        encoding.networkMemory.reservedUserspaceBytes == 0 ||
        encoding.networkMemory.maximumSocketBytes == 0 ||
        encoding.networkMemory.reservedUserspaceBytes >
            encoding.networkMemory.maximumTotalBytes ||
        encoding.networkMemory.maximumSocketBytes >
            encoding.networkMemory.maximumTotalBytes -
                encoding.networkMemory.reservedUserspaceBytes ||
        encoding.submitMode !=
            MediaDatagramSubmitMode::NonBlockingAtomicEnqueue ||
        encoding.orderingMode != MediaDatagramOrderingMode::CanonicalOrdered ||
        encoding.pressureFailureMode !=
            MediaDatagramLimitFailureMode::Terminate ||
        encoding.deadlineFailureMode !=
            MediaDatagramLimitFailureMode::Terminate ||
        encoding.persistentStateMode !=
            MediaDatagramPersistentStateMode::PreserveScopeDebt) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram shaping plan requires complete scope, service, resource, "
            "and "
            "failure facts"));
    }

    std::unordered_set<std::uint64_t> endpointIds;
    try {
        endpointIds.reserve(encoding.endpoints.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "datagram shaping endpoint identity validation"));
    }
    for (const auto& endpoint : encoding.endpoints) {
        auto valid = validateEndpoint(endpoint);
        if (!valid)
            return Result::failure(valid.error());
        auto maximumWireBytes = checkedWireBytes(
            endpoint, endpoint.maximumDatagramBytes);
        if (!maximumWireBytes) {
            return Result::failure(maximumWireBytes.error());
        }
        bool inserted = false;
        try {
            inserted = endpointIds.insert(endpoint.endpointId).second;
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "datagram shaping endpoint identity"));
        }
        if (!inserted) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram shaping endpoint identities must be unique"));
        }
        if (maximumWireBytes.value() > encoding.serviceCurve.burstWireBytes ||
            endpoint.maximumPendingDatagrams >
                encoding.backlog.maximumDatagrams ||
            endpoint.maximumPendingBytes > encoding.backlog.maximumBytes ||
            endpoint.maximumResidence > encoding.backlog.maximumResidence) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram endpoint bounds exceed the service-scope ledger"));
        }
    }
    auto scope = validateScopeCoverage(encoding.serviceScope, endpointIds);
    if (!scope)
        return Result::failure(scope.error());
    auto evidence = validateEvidence(encoding);
    if (!evidence)
        return Result::failure(evidence.error());
    return Result::success(MediaDatagramShapingPlan(std::move(encoding)));
}

MediaDatagramShapingPlan::MediaDatagramShapingPlan(
    MediaDatagramShapingPlanEncoding encoding) noexcept
    : m_encoding(std::move(encoding))
{}

::media::Result<MediaDatagramShapingPlanEncoding>
MediaDatagramShapingPlan::encode() const noexcept
{
    using Result = ::media::Result<MediaDatagramShapingPlanEncoding>;
    try {
        return Result::success(m_encoding);
    } catch (const std::bad_alloc&) {
        return Result::failure(
            ::media::ErrorInfo::allocationFailed("plan encode"));
    }
}

::media::Result<MediaDatagramShapingPlan>
MediaDatagramShapingPlan::clone() const noexcept
{
    using Result = ::media::Result<MediaDatagramShapingPlan>;
    try {
        auto encoding = encode();
        if (!encoding)
            return Result::failure(encoding.error());
        return decode(std::move(encoding).value());
    } catch (const std::bad_alloc&) {
        return Result::failure(
            ::media::ErrorInfo::allocationFailed("plan clone"));
    }
}

const std::string& MediaDatagramShapingPlan::sessionKey() const noexcept
{
    return m_encoding.sessionKey;
}

std::uint64_t MediaDatagramShapingPlan::generation() const noexcept
{
    return m_encoding.generation;
}

const MediaDatagramServiceScopePlan&
MediaDatagramShapingPlan::serviceScope() const noexcept
{
    return m_encoding.serviceScope;
}

const std::vector<MediaDatagramEndpointPlan>&
MediaDatagramShapingPlan::endpoints() const noexcept
{
    return m_encoding.endpoints;
}

const MediaDatagramEndpointPlan*
MediaDatagramShapingPlan::endpoint(std::uint64_t endpointId) const noexcept
{
    const auto found =
        std::find_if(m_encoding.endpoints.begin(), m_encoding.endpoints.end(),
                     [endpointId](const MediaDatagramEndpointPlan& endpoint) {
                         return endpoint.endpointId == endpointId;
                     });
    return found == m_encoding.endpoints.end() ? nullptr : &*found;
}

const MediaDatagramServiceCurvePlan&
MediaDatagramShapingPlan::serviceCurve() const noexcept
{
    return m_encoding.serviceCurve;
}

const MediaDatagramBacklogPlan&
MediaDatagramShapingPlan::backlog() const noexcept
{
    return m_encoding.backlog;
}

const MediaDatagramBatchPlan& MediaDatagramShapingPlan::batch() const noexcept
{
    return m_encoding.batch;
}

const MediaDatagramNetworkMemoryPlan&
MediaDatagramShapingPlan::networkMemory() const noexcept
{
    return m_encoding.networkMemory;
}

MediaDatagramSubmitMode MediaDatagramShapingPlan::submitMode() const noexcept
{
    return m_encoding.submitMode;
}

MediaDatagramOrderingMode
MediaDatagramShapingPlan::orderingMode() const noexcept
{
    return m_encoding.orderingMode;
}

MediaDatagramLimitFailureMode
MediaDatagramShapingPlan::pressureFailureMode() const noexcept
{
    return m_encoding.pressureFailureMode;
}

MediaDatagramLimitFailureMode
MediaDatagramShapingPlan::deadlineFailureMode() const noexcept
{
    return m_encoding.deadlineFailureMode;
}

MediaDatagramPersistentStateMode
MediaDatagramShapingPlan::persistentStateMode() const noexcept
{
    return m_encoding.persistentStateMode;
}

const std::optional<MediaDatagramTransmitEvidencePlan>&
MediaDatagramShapingPlan::evidence() const noexcept
{
    return m_encoding.evidence;
}

::media::Result<MediaDatagramPlannedWireCost>
MediaDatagramShapingPlan::plannedWireCost(
    std::uint64_t endpointId,
    std::uint64_t payloadBytes) const
{
    using Result = ::media::Result<MediaDatagramPlannedWireCost>;
    const auto* plannedEndpoint = endpoint(endpointId);
    if (!plannedEndpoint || payloadBytes == 0 ||
        payloadBytes > plannedEndpoint->maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram differs from its planned endpoint or MTU bound"));
    }

    auto wireBytes = checkedWireBytes(*plannedEndpoint, payloadBytes);
    if (!wireBytes) return Result::failure(wireBytes.error());
    auto pacingDurationNs = MediaCheckedArithmetic::ceilDurationNanoseconds(
        wireBytes.value(),
        m_encoding.serviceCurve.pacingWireBytesPerSecond,
        "datagram pacing debt duration");
    if (!pacingDurationNs) {
        return Result::failure(pacingDurationNs.error());
    }
    return Result::success(MediaDatagramPlannedWireCost{
        wireBytes.value(),
        MediaRunningTime::fromNanoseconds(pacingDurationNs.value())});
}

::media::Result<MediaDatagramWireDeadlinePlan>
MediaDatagramShapingPlan::wireDeadlinePlan(
    std::uint64_t endpointId) const noexcept
{
    using Result = ::media::Result<MediaDatagramWireDeadlinePlan>;
    const auto* plannedEndpoint = endpoint(endpointId);
    if (!plannedEndpoint) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire deadline endpoint is absent from the shaping plan"));
    }
    return Result::success(MediaDatagramWireDeadlinePlan{
        endpointId,
        (std::min)(plannedEndpoint->maximumResidence,
                   m_encoding.backlog.maximumResidence)});
}

} // namespace media::ffmpeg::graph
