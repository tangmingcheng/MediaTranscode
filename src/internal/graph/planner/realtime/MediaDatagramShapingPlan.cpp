#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"

#include "internal/graph/model/MediaNumericIpAddress.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status validateMtuEvidence(
    const MediaDatagramEndpointPlan& endpoint)
{
    const auto& evidence = endpoint.mtuEvidence;
    if (evidence.authority.empty() ||
        evidence.maximumIpPacketBytes == 0 ||
        evidence.ipHeaderBytes == 0 ||
        evidence.udpHeaderBytes == 0 ||
        evidence.senderMaximumPayloadBytes == 0 ||
        evidence.ipHeaderBytes >= evidence.maximumIpPacketBytes ||
        evidence.udpHeaderBytes >
            evidence.maximumIpPacketBytes - evidence.ipHeaderBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram endpoint requires complete MTU evidence"));
    }
    const auto networkPayloadBytes =
        evidence.maximumIpPacketBytes - evidence.ipHeaderBytes -
        evidence.udpHeaderBytes;
    const auto derivedMaximum = (std::min)(
        networkPayloadBytes, evidence.senderMaximumPayloadBytes);
    if (endpoint.maximumDatagramBytes == 0 ||
        endpoint.maximumDatagramBytes != derivedMaximum) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram endpoint maximum payload differs from its MTU evidence"));
    }
    return ::media::Status::success();
}

::media::Status validateEndpoint(const MediaDatagramEndpointPlan& endpoint)
{
    auto address = MediaNumericIpAddress::create(
        endpoint.addressFamily, endpoint.numericAddress);
    auto mtu = validateMtuEvidence(endpoint);
    if (endpoint.endpointId == 0 || endpoint.port == 0 || !address || !mtu ||
        endpoint.maximumPendingDatagrams == 0 ||
        endpoint.maximumPendingBytes < endpoint.maximumDatagramBytes ||
        endpoint.maximumResidence <= MediaRunningTime::fromNanoseconds(0) ||
        endpoint.socketHardBoundBytes < endpoint.maximumDatagramBytes) {
        return ::media::Status::failure(
            !address ? address.error()
                     : (!mtu ? mtu.error()
                             : ::media::ErrorInfo::invalidArgument(
                                   "datagram endpoint hard bounds are incomplete")));
    }
    return ::media::Status::success();
}

::media::Status validateEvidence(
    const std::optional<MediaDatagramTransmitEvidencePlan>& evidence)
{
    if (!evidence) return ::media::Status::success();
    const bool knownKind =
        evidence->kind ==
            MediaDatagramTransmitEvidenceKind::TransmitTimestamp ||
        evidence->kind ==
            MediaDatagramTransmitEvidenceKind::ZeroCopyCompletion ||
        evidence->kind == MediaDatagramTransmitEvidenceKind::
                              TransmitTimestampAndZeroCopyCompletion;
    if (!knownKind || evidence->authority.empty() ||
        evidence->firstEvidenceId == 0 ||
        evidence->lastEvidenceId < evidence->firstEvidenceId ||
        evidence->maximumCorrelationEntries == 0 ||
        evidence->maximumDrainResidence <=
            MediaRunningTime::fromNanoseconds(0) ||
        evidence->lastEvidenceId - evidence->firstEvidenceId ==
            (std::numeric_limits<std::uint64_t>::max)() ||
        evidence->lastEvidenceId - evidence->firstEvidenceId + 1 <
            evidence->maximumCorrelationEntries) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "datagram transmit evidence plan is incomplete"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaDatagramShapingPlan>
MediaDatagramShapingPlan::decode(
    MediaDatagramShapingPlanEncoding encoding)
{
    using Result = ::media::Result<MediaDatagramShapingPlan>;
    if (encoding.sessionKey.empty() || encoding.generation == 0 ||
        encoding.serviceScopeId.empty() || encoding.endpoints.empty() ||
        encoding.serviceCurve.authority.empty() ||
        encoding.serviceCurve.serviceBytesPerSecond == 0 ||
        encoding.serviceCurve.peakBytesPerSecond <
            encoding.serviceCurve.serviceBytesPerSecond ||
        encoding.serviceCurve.burstBytes == 0 ||
        encoding.backlog.maximumDatagrams == 0 ||
        encoding.backlog.maximumBytes == 0 ||
        encoding.backlog.maximumResidence <=
            MediaRunningTime::fromNanoseconds(0) ||
        encoding.batch.maximumDatagrams == 0 ||
        encoding.batch.maximumBytes == 0 ||
        encoding.batch.maximumDatagrams >
            encoding.backlog.maximumDatagrams ||
        encoding.batch.maximumBytes > encoding.backlog.maximumBytes ||
        encoding.submitMode !=
            MediaDatagramSubmitMode::NonBlockingAtomicEnqueue ||
        encoding.orderingMode !=
            MediaDatagramOrderingMode::CanonicalOrdered ||
        encoding.pressureFailureMode !=
            MediaDatagramLimitFailureMode::Terminate ||
        encoding.deadlineFailureMode !=
            MediaDatagramLimitFailureMode::Terminate ||
        encoding.persistentStateMode !=
            MediaDatagramPersistentStateMode::PreserveScopeDebt) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram shaping plan requires complete scope, service, resource, and failure facts"));
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
        if (!valid) return Result::failure(valid.error());
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
        if (endpoint.maximumDatagramBytes >
                encoding.serviceCurve.burstBytes ||
            endpoint.maximumPendingDatagrams >
                encoding.backlog.maximumDatagrams ||
            endpoint.maximumPendingBytes > encoding.backlog.maximumBytes ||
            endpoint.maximumResidence > encoding.backlog.maximumResidence) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram endpoint bounds exceed the service-scope ledger"));
        }
    }
    auto evidence = validateEvidence(encoding.evidence);
    if (!evidence) return Result::failure(evidence.error());
    return Result::success(MediaDatagramShapingPlan(std::move(encoding)));
}

MediaDatagramShapingPlan::MediaDatagramShapingPlan(
    MediaDatagramShapingPlanEncoding encoding) noexcept
    : m_encoding(std::move(encoding))
{
}

MediaDatagramShapingPlanEncoding MediaDatagramShapingPlan::encode() const
{
    return m_encoding;
}

::media::Result<MediaDatagramShapingPlan>
MediaDatagramShapingPlan::clone() const
{
    return decode(encode());
}

const std::string& MediaDatagramShapingPlan::sessionKey() const noexcept
{
    return m_encoding.sessionKey;
}

std::uint64_t MediaDatagramShapingPlan::generation() const noexcept
{
    return m_encoding.generation;
}

const std::string& MediaDatagramShapingPlan::serviceScopeId() const noexcept
{
    return m_encoding.serviceScopeId;
}

const std::vector<MediaDatagramEndpointPlan>&
MediaDatagramShapingPlan::endpoints() const noexcept
{
    return m_encoding.endpoints;
}

const MediaDatagramEndpointPlan* MediaDatagramShapingPlan::endpoint(
    std::uint64_t endpointId) const noexcept
{
    const auto found = std::find_if(
        m_encoding.endpoints.begin(), m_encoding.endpoints.end(),
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

const MediaDatagramBatchPlan&
MediaDatagramShapingPlan::batch() const noexcept
{
    return m_encoding.batch;
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

::media::Status MediaDatagramShapingPlan::validateDatagram(
    std::uint64_t endpointId, std::uint64_t payloadBytes) const
{
    const auto* plannedEndpoint = endpoint(endpointId);
    if (!plannedEndpoint || payloadBytes == 0 ||
        payloadBytes > plannedEndpoint->maximumDatagramBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "wire datagram differs from its planned endpoint or MTU bound"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
