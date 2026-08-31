#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::uint64_t ceilDivide(std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value / divisor + (value % divisor != 0 ? 1U : 0U);
}

struct EvidenceEntryGeometry final {
    std::uint64_t endpointId;
    std::uint64_t evidenceId;
    std::uint32_t platformCorrelationId;
    std::optional<std::uint32_t> launchTimeLowBits;
    std::optional<MediaRunningTime> launchCorrelationRetainUntil;
    MediaRunningTime submittedAt;
    std::uint8_t state;
    bool timestampExpected;
    bool timestampObserved;
};

::media::Result<std::uint64_t> containers(
    std::uint64_t count, std::uint64_t bytesPerItem, const char* fact)
{
    return MediaCheckedArithmetic::multiply(
        count, bytesPerItem, fact);
}

} // namespace

::media::Result<MediaRealtimeNetworkResourceLedgerPlan>
MediaRealtimeNetworkResourceLedgerPlanner::plan(
    const MediaRealtimeDeploymentLatencyBudget& latency,
    const MediaRealtimeDeploymentObservationBudget& observation,
    const MediaWireTrafficEnvelope& wire,
    std::uint64_t endpointCount)
{
    using Result = ::media::Result<MediaRealtimeNetworkResourceLedgerPlan>;
    if (endpointCount == 0 || wire.burstDatagrams == 0 ||
        wire.maximumAtomicDatagrams == 0 ||
        wire.maximumAtomicWireBytes == 0 ||
        wire.maximumWireDatagramBytes == 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "network ledger requires endpoint and wire geometry"));
    }
    auto residenceBytes = MediaCheckedArithmetic::bytesForResidence(
        wire.peakWireBytesPerSecond,
        latency.maximumResidence.nanoseconds(),
        "network residence payload");
    auto residenceDatagrams = MediaCheckedArithmetic::bytesForResidence(
        wire.peakDatagramsPerSecond,
        latency.maximumResidence.nanoseconds(),
        "network residence datagrams");
    auto serviceBacklogBytes = residenceBytes
        ? MediaCheckedArithmetic::add(
              residenceBytes.value(), wire.burstWireBytes,
              "network backlog payload")
        : residenceBytes;
    if (!residenceDatagrams || !serviceBacklogBytes) {
        return Result::failure(
            !residenceDatagrams ? residenceDatagrams.error() :
            serviceBacklogBytes.error());
    }
    const auto backlogBytes = (std::max)(
        serviceBacklogBytes.value(), wire.maximumAtomicWireBytes);
    const auto backlogDatagrams = (std::max)(
        (std::max)(
            (std::max)(
                ceilDivide(backlogBytes, wire.maximumWireDatagramBytes),
                wire.maximumAtomicDatagrams),
            residenceDatagrams.value()),
        wire.burstDatagrams);
    const auto batchBytes = (std::min)(
        serviceBacklogBytes.value(),
        (std::max)(wire.burstWireBytes, wire.maximumWireDatagramBytes));
    const auto batchDatagrams = ceilDivide(
        batchBytes, wire.maximumWireDatagramBytes);
    const auto socketPerEndpoint = (std::max)(
        wire.maximumWireDatagramBytes, serviceBacklogBytes.value());
    // An endpoint may be the sole destination of the next atomic protocol
    // transaction.  Its userspace pending bound therefore has to admit the
    // complete service-scope backlog; the aggregate backlog still prevents
    // all endpoints from consuming that bound simultaneously.  Kernel socket
    // sizing remains a separate service-residence product and must not enlarge
    // the userspace service curve.
    const auto endpointPendingBytes = backlogBytes;
    const auto endpointPendingDatagrams = backlogDatagrams;
    const auto correlationEntries =
        observation.evidencePolicy ==
                MediaRealtimeTransmitEvidencePolicy::Disabled
            ? 0U
            : (std::min)(observation.maximumRunDatagrams,
                         backlogDatagrams);
    if (backlogDatagrams == 0 || batchDatagrams == 0 ||
        socketPerEndpoint < wire.maximumWireDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "network and socket budgets cannot admit one datagram per endpoint"));
    }

    const auto backlogContainerUnit = static_cast<std::uint64_t>(
        sizeof(MediaWireDatagram) +
        sizeof(std::shared_ptr<MediaWireDatagramBatchBuffer>));
    const auto batchContainerUnit = static_cast<std::uint64_t>(
        sizeof(MediaWireDatagramDescriptor));
    const auto endpointContainerUnit = static_cast<std::uint64_t>(
        sizeof(MediaWireDatagram*));
    const auto evidenceUnit = static_cast<std::uint64_t>(
        sizeof(EvidenceEntryGeometry) +
        sizeof(std::pair<const std::uint64_t, std::uint64_t>) +
        sizeof(std::pair<const std::uint32_t, std::uint64_t>) * 2U +
        sizeof(void*) * 6U);
    auto backlogContainers = containers(
        backlogDatagrams, backlogContainerUnit, "backlog containers");
    auto batchContainers = containers(
        batchDatagrams, batchContainerUnit, "batch containers");
    auto endpointItems = MediaCheckedArithmetic::multiply(
        endpointPendingDatagrams, endpointCount,
        "endpoint pending container count");
    auto endpointContainers = endpointItems
        ? containers(endpointItems.value(), endpointContainerUnit,
                     "endpoint pending containers")
        : endpointItems;
    auto evidenceContainers = containers(
        correlationEntries, evidenceUnit, "evidence correlation containers");
    auto withBacklogContainers = backlogContainers
        ? MediaCheckedArithmetic::add(
              backlogBytes, backlogContainers.value(),
              "backlog payload and containers")
        : backlogContainers;
    auto withBatch = withBacklogContainers && batchContainers
        ? MediaCheckedArithmetic::add(
              withBacklogContainers.value(), batchContainers.value(),
              "network batch containers")
        : (!withBacklogContainers ? withBacklogContainers : batchContainers);
    auto withEndpoints = withBatch && endpointContainers
        ? MediaCheckedArithmetic::add(
              withBatch.value(), endpointContainers.value(),
              "network endpoint containers")
        : (!withBatch ? withBatch : endpointContainers);
    auto networkBytes = withEndpoints && evidenceContainers
        ? MediaCheckedArithmetic::add(
              withEndpoints.value(), evidenceContainers.value(),
              "network evidence containers")
        : (!withEndpoints ? withEndpoints : evidenceContainers);
    auto socketBytes = MediaCheckedArithmetic::multiply(
        socketPerEndpoint, endpointCount, "aggregate socket kernel buffers");
    if (!networkBytes || !socketBytes) {
        return Result::failure(
            !networkBytes ? networkBytes.error() :
            !socketBytes ? socketBytes.error() :
            ::media::ErrorInfo::invalidArgument(
                "network resource ledger arithmetic failed"));
    }
    try {
        std::vector<MediaRealtimeNetworkResourceLedgerEntry> entries{
            {MediaRealtimeNetworkAccountingGroup::BacklogPayload,
             backlogDatagrams, backlogBytes, false,
             "wire-residence+burst-or-atomic-transaction-shared-payload"},
            {MediaRealtimeNetworkAccountingGroup::BacklogContainer,
             backlogDatagrams, backlogContainers.value(), false,
             "scheduled-datagram-container-abi"},
            {MediaRealtimeNetworkAccountingGroup::BatchContainer,
             batchDatagrams, batchContainers.value(), false,
             "batch-descriptor-shares-backlog-payload"},
            {MediaRealtimeNetworkAccountingGroup::EndpointPendingContainer,
             endpointItems.value(), endpointContainers.value(), false,
             "endpoint-pending-view-shares-backlog-payload"},
            {MediaRealtimeNetworkAccountingGroup::EvidenceCorrelation,
             correlationEntries, evidenceContainers.value(), false,
             "evidence-entry+hash-node+bucket-abi"},
            {MediaRealtimeNetworkAccountingGroup::SocketKernelBuffer,
             endpointCount, socketBytes.value(), true,
             "deployment-socket-budget"}};
        MediaRealtimeNetworkResourceLedgerPlan ledger{
            backlogDatagrams, backlogBytes,
            batchDatagrams, batchBytes,
            endpointPendingDatagrams, endpointPendingBytes,
            socketPerEndpoint, socketPerEndpoint, socketPerEndpoint,
            correlationEntries,
            networkBytes.value(), socketBytes.value(), std::move(entries)};
        auto status = validate(ledger);
        return status ? Result::success(std::move(ledger))
                      : Result::failure(status.error());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "network resource ledger"));
    }
}

::media::Status MediaRealtimeNetworkResourceLedgerPlanner::validate(
    const MediaRealtimeNetworkResourceLedgerPlan& ledger)
{
    if (ledger.entries.size() != 6 || ledger.maximumBacklogDatagrams == 0 ||
        ledger.maximumBacklogBytes == 0 ||
        ledger.maximumBatchDatagrams == 0 || ledger.maximumBatchBytes == 0 ||
        ledger.maximumEndpointPendingDatagrams == 0 ||
        ledger.maximumEndpointPendingBytes == 0 ||
        ledger.targetEffectiveSendBufferBytesPerEndpoint == 0 ||
        ledger.minimumEffectiveSendBufferBytesPerEndpoint == 0 ||
        ledger.maximumAdmittedEffectiveSendBufferBytesPerEndpoint <
            ledger.targetEffectiveSendBufferBytesPerEndpoint) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "network resource ledger is incomplete"));
    }
    std::uint64_t network = 0;
    std::uint64_t socket = 0;
    for (const auto& entry : ledger.entries) {
        if (entry.itemCount == 0 && entry.group !=
                MediaRealtimeNetworkAccountingGroup::EvidenceCorrelation) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "network ledger entry item count is zero"));
        }
        auto next = MediaCheckedArithmetic::add(
            entry.chargedToSocketBudget ? socket : network,
            entry.bytes, "network ledger validation");
        if (!next) return ::media::Status::failure(next.error());
        (entry.chargedToSocketBudget ? socket : network) = next.value();
    }
    if (network != ledger.admittedNetworkBytes ||
        socket != ledger.admittedSocketBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "network resource ledger totals conflict with deployment budgets"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
