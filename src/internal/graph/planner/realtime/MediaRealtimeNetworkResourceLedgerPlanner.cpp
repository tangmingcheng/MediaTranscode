#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include "internal/graph/nodes/output/MediaRtpWirePreparationWorkspace.h"
#include "internal/graph/nodes/output/MediaMpegTsUdpWirePreparationWorkspace.h"
#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"

#include "internal/graph/planner/realtime/MediaDatagramSocketBufferPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"
#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"
#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include <limits>
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

struct RtpStorageGeometry final {
    std::uint64_t instances;
    std::uint64_t recordBytes;
    std::uint64_t tableBytes;
};

::media::Result<RtpStorageGeometry> rtpStorageGeometry(
    MediaDatagramProtocolLayout layout, std::uint64_t recordCapacity)
{
    using Result = ::media::Result<RtpStorageGeometry>;
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return Result::failure(topology.error());
    const auto instances = topology.value().rtpInstances.size();
    if (!instances) return Result::success({0, 0, 0});
    auto records = MediaRtpWireProtocolState::reservationStorageFootprint(recordCapacity);
    auto table = MediaRtpProtocolStorageRegistry::storageFootprint(instances);
    if (!records || !table) return Result::failure(!records ? records.error() : table.error());
    auto total = MediaCheckedArithmetic::multiply(instances, records.value(), "RTP record allocations");
    if (!total) return Result::failure(total.error());
    return Result::success({instances, total.value(), table.value()});
}

::media::Result<MediaDatagramCommitStoragePlan> commitStoragePlan(MediaDatagramProtocolLayout layout)
{
    using Result = ::media::Result<MediaDatagramCommitStoragePlan>;
    auto batchGeometry = MediaWireDatagramBatchBuffer::allocationGeometry();
    if (!batchGeometry) return Result::failure(batchGeometry.error());
    const auto prefix = layout == MediaDatagramProtocolLayout::MpegTsUdp ||
        layout == MediaDatagramProtocolLayout::MpegTsRtp
        ? std::optional{MediaMpegTsProtocolDatagramBatchBuffer::prefixAllocationGeometry()}
        : std::nullopt;
    switch (layout) {
    case MediaDatagramProtocolLayout::VideoRtp:
    case MediaDatagramProtocolLayout::SeparateAvRtp:
    case MediaDatagramProtocolLayout::MpegTsRtp: {
        auto geometry = MediaRtpWireCommitTransaction::allocationGeometry();
        if (!geometry) return Result::failure(geometry.error());
        return Result::success({MediaDatagramCommitStorageKind::Rtp, geometry.value(), batchGeometry.value(), prefix});
    }
    case MediaDatagramProtocolLayout::MpegTsUdp: {
        auto geometry = MediaMpegTsUdpWireDatagramMaterializer::transactionAllocationGeometry();
        if (!geometry) return Result::failure(geometry.error());
        return Result::success({MediaDatagramCommitStorageKind::MpegTsUdp, geometry.value(), batchGeometry.value(), prefix});
    }
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument("Unknown commit storage protocol"));
    }
}
::media::Result<std::optional<MediaWireProducerStoragePlan>> mpegTsStoragePlan(
    const std::optional<MediaMpegTsProducerPlan>& producer,
    const MediaDatagramCommitStoragePlan& commit, const MediaDatagramBatchPlan& batch)
{
    using Result = ::media::Result<std::optional<MediaWireProducerStoragePlan>>;
    if (!producer) return Result::success(std::nullopt);
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    const bool rtp = commit.kind == MediaDatagramCommitStorageKind::Rtp;
    const auto count = batch.maximumDatagrams;
    const auto bytes = producer->maximumDatagramBytes;
    if (!count || count >= maximum || !bytes || count > maximum / bytes || batch.maximumBytes < bytes)
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS storage batch geometry is invalid"));
    const auto entries = static_cast<std::size_t>(count) + (rtp ? 1 : 0);
    auto payload = MediaCheckedArithmetic::add(count * bytes, producer->maximumRtcpCompoundBytes,
        "MPEG-TS prepared payload");
    if (!payload) return Result::failure(payload.error());
    if (payload.value() > maximum) return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS payload exceeds native size"));
    auto workspace = rtp ? MediaRtpWirePreparationWorkspace::storageFootprint(static_cast<std::size_t>(count), entries)
        : MediaMpegTsUdpWirePreparationWorkspace::storageFootprint(entries);
    if (!workspace) return Result::failure(workspace.error());
    std::optional<std::size_t> terminal;
    if (rtp) {
        auto size = MediaCheckedArithmetic::add(producer->maximumRtcpCompoundBytes,
            MediaRtcpWireGeometry::byePayloadBytes(), "MPEG-TS terminal payload");
        if (!size) return Result::failure(size.error());
        if (size.value() > bytes) return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS terminal exceeds MTU"));
        terminal = static_cast<std::size_t>(size.value());
    }
    auto plan = MediaWireProducerStoragePlanner::plan(
        {entries, static_cast<std::size_t>(payload.value()), workspace.value(), 0, terminal}, commit, batch);
    return plan ? Result::success(plan.value()) : Result::failure(plan.error());
}
::media::Result<std::uint64_t> mpegTsOwnedBytes(const std::optional<MediaWireProducerStoragePlan>& plan,
    const std::optional<MediaMpegTsProducerPlan>& producer)
{
    if (!plan) return ::media::Result<std::uint64_t>::success(0);
    auto slots = MediaCheckedArithmetic::add(plan->producerSlotBytes, plan->activeTransactionBytes, "MPEG-TS prepared and active owners");
    auto owned = slots ? MediaCheckedArithmetic::add(slots.value(), plan->senderHandoffBytes, "MPEG-TS sender handoff") : slots;
    if (!owned || !producer || !producer->muxQueueStorage) return owned;
    owned = MediaCheckedArithmetic::add(owned.value(), producer->muxQueueStorage->allocationBytes,
        "MPEG-TS final mux queue backing");
    if (!owned || !producer->seriesStorage) return owned;
    return MediaCheckedArithmetic::add(owned.value(), producer->seriesStorage->allocationBytes,
        "MPEG-TS final series storage");
}

::media::Result<std::vector<MediaRawRtpStoragePlan>> rawStoragePlans(
    std::span<const MediaRawRtpProducerPlan> producers,
    const MediaDatagramCommitStoragePlan& commit, const MediaDatagramBatchPlan& batch)
{
    using Result = ::media::Result<std::vector<MediaRawRtpStoragePlan>>;
    try {
        std::vector<MediaRawRtpStoragePlan> result;
        result.reserve(producers.size());
        for (const auto& producer : producers) {
            auto storage = MediaRawRtpStoragePlanner::plan(producer, commit, batch);
            if (!storage) return Result::failure(storage.error());
            result.push_back(storage.value());
        }
        return Result::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Raw RTP storage planning"));
    }
}
} // namespace

::media::Result<MediaRealtimeNetworkResourceLedgerPlan>
MediaRealtimeNetworkResourceLedgerPlanner::plan(
    const MediaRealtimeDeploymentLatencyBudget& latency,
    const MediaRealtimeDeploymentObservationBudget& observation,
    const MediaWireTrafficEnvelope& wire,
    MediaIpAddressFamily addressFamily,
    MediaDatagramProtocolLayout protocolLayout,
    std::span<const MediaRawRtpProducerPlan> rawRtpProducers,
    const std::optional<MediaMpegTsProducerPlan>& mpegTsProducer)
{
    using Result = ::media::Result<MediaRealtimeNetworkResourceLedgerPlan>;
    auto mpegTsFacts = validateMpegTsProducerPlan(protocolLayout, mpegTsProducer);
    if (!mpegTsFacts) return Result::failure(mpegTsFacts.error());
    auto producerFacts = validateRawRtpProducerPlans(protocolLayout, rawRtpProducers);
    if (!producerFacts) return Result::failure(producerFacts.error());
    auto topology = mediaDatagramProtocolTopology(protocolLayout);
    if (!topology) return Result::failure(topology.error());
    const auto endpointCount = topology.value().endpointRoles.size();
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

    const auto endpointContainerUnit = static_cast<std::uint64_t>(
        sizeof(MediaWireDatagram*));
    const auto evidenceUnit = static_cast<std::uint64_t>(
        sizeof(EvidenceEntryGeometry) +
        sizeof(std::pair<const std::uint64_t, std::uint64_t>) +
        sizeof(std::pair<const std::uint32_t, std::uint64_t>) * 2U +
        sizeof(void*) * 6U);
    const bool raw = !rawRtpProducers.empty();
    auto backlogContainers = ::media::Result<std::uint64_t>::success(0);
    auto commitStorage = commitStoragePlan(protocolLayout);
    if (!commitStorage) return Result::failure(commitStorage.error());
    auto rawStorage = rawStoragePlans(rawRtpProducers, commitStorage.value(), {batchDatagrams, batchBytes});
    if (!rawStorage) return Result::failure(rawStorage.error());
    auto rawOwned = MediaRawRtpStoragePlanner::ownedStorageUpperBound(rawStorage.value());
    if (!rawOwned) return Result::failure(rawOwned.error());
    auto mpegStorage = mpegTsStoragePlan(mpegTsProducer, commitStorage.value(), {batchDatagrams, batchBytes});
    if (!mpegStorage) return Result::failure(mpegStorage.error());
    auto mpegOwned = mpegTsOwnedBytes(mpegStorage.value(), mpegTsProducer);
    if (!mpegOwned) return Result::failure(mpegOwned.error());
    if (mpegTsProducer && (batchDatagrams > backlogDatagrams ||
        (protocolLayout == MediaDatagramProtocolLayout::MpegTsRtp && batchDatagrams >= backlogDatagrams)))
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS atomic transaction exceeds planned backlog"));
    for (const auto& producer : rawStorage.value()) {
        if (producer.maximumMediaDatagrams >= backlogDatagrams)
            return Result::failure(::media::ErrorInfo::invalidArgument("Raw RTP atomic transaction exceeds planned backlog"));
    }
    auto batchContainers = ::media::Result<std::size_t>::success(0);
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
              raw ? rawOwned.value() : mpegOwned.value(), backlogContainers.value(),
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
    auto globalRecords = MediaWireGlobalSequenceState::reservationStorageFootprint(
        backlogDatagrams);
    if (!globalRecords) return Result::failure(globalRecords.error());
    if (networkBytes) {
        networkBytes = MediaCheckedArithmetic::add(
            networkBytes.value(), globalRecords.value(), "global sequence record storage");
    }
    auto rtpStorage = rtpStorageGeometry(protocolLayout, backlogDatagrams);
    if (!rtpStorage) return Result::failure(rtpStorage.error());
    for (const auto bytes : {rtpStorage.value().recordBytes, rtpStorage.value().tableBytes}) {
        if (networkBytes) networkBytes = MediaCheckedArithmetic::add(
            networkBytes.value(), bytes, "RTP protocol fixed storage");
    }
    auto controlBytes = ::media::Result<std::uint64_t>::success(0);
    if (!controlBytes) return Result::failure(controlBytes.error());
    if (networkBytes) networkBytes = MediaCheckedArithmetic::add(
        networkBytes.value(), controlBytes.value(), "network commit control blocks");
    auto reservationBytes = ::media::Result<std::uint64_t>::success(0);
    if (!reservationBytes) return Result::failure(reservationBytes.error());
    if (networkBytes) networkBytes = MediaCheckedArithmetic::add(
        networkBytes.value(), reservationBytes.value(), "network commit reservation storage");
    auto capability =
        MediaDatagramSocketBufferPlatformCapabilityProbe::scan(
            socketPerEndpoint, addressFamily);
    auto socketBuffer = capability
        ? MediaDatagramSocketBufferPlanner::plan(
              socketPerEndpoint, capability.value())
        : ::media::Result<MediaDatagramSocketBufferPlan>::failure(
              capability.error());
    auto socketBytes = socketBuffer
        ? MediaCheckedArithmetic::multiply(
              socketBuffer.value().maximumAdmittedEffectiveBytes,
              endpointCount, "aggregate socket kernel buffers")
        : ::media::Result<std::uint64_t>::failure(socketBuffer.error());
    if (!networkBytes || !socketBuffer || !socketBytes) {
        return Result::failure(
            !networkBytes ? networkBytes.error() :
            !socketBuffer ? socketBuffer.error() :
            !socketBytes ? socketBytes.error() :
            ::media::ErrorInfo::invalidArgument(
                "network resource ledger arithmetic failed"));
    }
    try {
        std::vector<MediaRealtimeNetworkResourceLedgerEntry> entries{
            {MediaRealtimeNetworkAccountingGroup::BacklogPayload,
             backlogDatagrams, 0, false,
             "wire-residence+burst-or-atomic-transaction-shared-payload"},
            {MediaRealtimeNetworkAccountingGroup::BacklogContainer,
             backlogDatagrams, backlogContainers.value(), false,
             "scheduled-datagram-container-abi"},
            {MediaRealtimeNetworkAccountingGroup::BatchContainer,
             backlogDatagrams, batchContainers.value(), false,
             "exact-final-output-metadata-and-allocation-headers"},
            {MediaRealtimeNetworkAccountingGroup::EndpointPendingContainer,
             endpointItems.value(), endpointContainers.value(), false,
             "endpoint-pending-view-shares-backlog-payload"},
            {MediaRealtimeNetworkAccountingGroup::EvidenceCorrelation,
             correlationEntries, evidenceContainers.value(), false,
             "evidence-entry+hash-node+bucket-abi"},
            {MediaRealtimeNetworkAccountingGroup::SocketKernelBuffer,
             endpointCount, socketBytes.value(), true,
             "deployment-socket-budget"},
            {MediaRealtimeNetworkAccountingGroup::GlobalSequenceRecords,
             backlogDatagrams, globalRecords.value(), false,
             "global-sequence-fixed-record-allocation-footprint"},
            {MediaRealtimeNetworkAccountingGroup::RtpProtocolRecords,
             rtpStorage.value().instances, rtpStorage.value().recordBytes, false,
             "per-rtp-instance-fixed-record-allocation-footprint"},
            {MediaRealtimeNetworkAccountingGroup::RtpInstanceTable,
             rtpStorage.value().instances, rtpStorage.value().tableBytes, false,
             "rtp-instance-registry-fixed-allocation-footprint"},
            {MediaRealtimeNetworkAccountingGroup::CommitControlBlocks,
             backlogDatagrams, controlBytes.value(), false,
             "probed-model-control-block-shared-root-contribution"},
            {MediaRealtimeNetworkAccountingGroup::CommitReservationStorage,
             backlogDatagrams, reservationBytes.value(), false,
             "exact-action-and-global-array-allocation-footprints"}};
        if (raw) entries.push_back({MediaRealtimeNetworkAccountingGroup::RawRtpOwnedStorage,
            rawStorage.value().size(), rawOwned.value(), false,
            "serial-producer-slots+active-transaction+sender-handoff-allocation-footprints"});
        if (!raw) entries.push_back({MediaRealtimeNetworkAccountingGroup::MpegTsOwnedStorage,
            1, mpegOwned.value(), false,
            "serial-producer-slot+active-transaction+sender-handoff-allocation-footprints"});
        MediaRealtimeNetworkResourceLedgerPlan ledger{
            protocolLayout, commitStorage.value(), backlogDatagrams, backlogBytes,
            batchDatagrams, batchBytes,
            endpointPendingDatagrams, endpointPendingBytes,
            socketBuffer.value(),
            correlationEntries,
            networkBytes.value(), socketBytes.value(), std::move(entries), true,
            std::vector<MediaRawRtpProducerPlan>(rawRtpProducers.begin(), rawRtpProducers.end()),
            std::move(rawStorage).value(), mpegTsProducer, std::move(mpegStorage).value()};
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
    auto socketBuffer =
        validateMediaDatagramSocketBufferPlan(ledger.socketBufferPerEndpoint);
    if (!socketBuffer) return socketBuffer;
    auto producerFacts = validateRawRtpProducerPlans(ledger.protocolLayout, ledger.rawRtpProducers);
    if (!producerFacts) return producerFacts;
    auto mpegTsFacts = validateMpegTsProducerPlan(ledger.protocolLayout, ledger.mpegTsProducer);
    if (!mpegTsFacts) return mpegTsFacts;
    const bool raw = !ledger.rawRtpProducers.empty();
    if (ledger.entries.size() != 12 || !ledger.commitBeforeNextReservation ||
        ledger.maximumBacklogDatagrams == 0 ||
        ledger.maximumBacklogBytes == 0 ||
        ledger.maximumBatchDatagrams == 0 || ledger.maximumBatchBytes == 0 ||
        ledger.maximumEndpointPendingDatagrams == 0 ||
        ledger.maximumEndpointPendingBytes == 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "network resource ledger is incomplete"));
    }
    auto globalRecords = MediaWireGlobalSequenceState::reservationStorageFootprint(
        ledger.maximumBacklogDatagrams);
    if (!globalRecords) return ::media::Status::failure(globalRecords.error());
    auto rtpStorage = rtpStorageGeometry(ledger.protocolLayout, ledger.maximumBacklogDatagrams);
    if (!rtpStorage) return ::media::Status::failure(rtpStorage.error());
    auto commitStorage = commitStoragePlan(ledger.protocolLayout);
    if (!commitStorage) return ::media::Status::failure(commitStorage.error());
    if (ledger.commitStorage.kind != commitStorage.value().kind ||
        ledger.commitStorage.geometry != commitStorage.value().geometry ||
        ledger.commitStorage.batchGeometry != commitStorage.value().batchGeometry ||
        ledger.commitStorage.protocolPrefixGeometry != commitStorage.value().protocolPrefixGeometry)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Commit storage differs from the current platform capability"));
    auto storage = rawStoragePlans(ledger.rawRtpProducers, ledger.commitStorage,
        {ledger.maximumBatchDatagrams, ledger.maximumBatchBytes});
    if (!storage) return ::media::Status::failure(storage.error());
    if (storage.value() != ledger.rawRtpStorage)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Raw RTP storage products differ from producer facts"));
    auto rawOwned = MediaRawRtpStoragePlanner::ownedStorageUpperBound(storage.value());
    if (!rawOwned) return ::media::Status::failure(rawOwned.error());
    auto mpegStorage = mpegTsStoragePlan(ledger.mpegTsProducer, ledger.commitStorage,
        {ledger.maximumBatchDatagrams, ledger.maximumBatchBytes});
    if (!mpegStorage) return ::media::Status::failure(mpegStorage.error());
    if (mpegStorage.value() != ledger.mpegTsStorage)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MPEG-TS storage differs from producer facts"));
    auto mpegOwned = mpegTsOwnedBytes(mpegStorage.value(), ledger.mpegTsProducer);
    if (!mpegOwned) return ::media::Status::failure(mpegOwned.error());
    auto controlBytes = ::media::Result<std::uint64_t>::success(0);
    auto reservationBytes = ::media::Result<std::uint64_t>::success(0);
    auto outputOverhead = ::media::Result<std::size_t>::success(0);
    bool foundOutput = false;
    bool foundReservation = false;
    bool foundControl = false;
    bool foundRtpRecords = false;
    bool foundRtpTable = false;
    bool foundGlobalRecords = false;
    bool foundRawOwned = false;
    bool foundMpegOwned = false;
    std::uint64_t network = 0;
    std::uint64_t socket = 0;
    for (const auto& entry : ledger.entries) {
        if ((entry.group == MediaRealtimeNetworkAccountingGroup::BacklogPayload ||
                    entry.group == MediaRealtimeNetworkAccountingGroup::BacklogContainer) && entry.bytes != 0)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Wire producer storage was charged twice"));
        if (entry.group == MediaRealtimeNetworkAccountingGroup::MpegTsOwnedStorage) {
            if (raw || foundMpegOwned || entry.chargedToSocketBudget || entry.itemCount != 1 || entry.bytes != mpegOwned.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MPEG-TS owner ledger differs from storage products"));
            foundMpegOwned = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::RawRtpOwnedStorage) {
            if (!raw || foundRawOwned || entry.chargedToSocketBudget ||
                entry.itemCount != storage.value().size() || entry.bytes != rawOwned.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Raw RTP owner ledger differs from its storage products"));
            foundRawOwned = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::BatchContainer) {
            if (foundOutput || entry.chargedToSocketBudget ||
                entry.itemCount != ledger.maximumBacklogDatagrams || entry.bytes != outputOverhead.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Wire output storage differs from its planned geometry contribution"));
            foundOutput = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::RtpProtocolRecords ||
            entry.group == MediaRealtimeNetworkAccountingGroup::RtpInstanceTable) {
            const bool records = entry.group == MediaRealtimeNetworkAccountingGroup::RtpProtocolRecords;
            auto& found = records ? foundRtpRecords : foundRtpTable;
            const auto bytes = records ? rtpStorage.value().recordBytes : rtpStorage.value().tableBytes;
            if (found || entry.chargedToSocketBudget ||
                entry.itemCount != rtpStorage.value().instances || entry.bytes != bytes)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "RTP fixed storage ledger conflicts with planned protocol instances"));
            found = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::CommitControlBlocks) {
            if (foundControl || entry.chargedToSocketBudget ||
                entry.itemCount != ledger.maximumBacklogDatagrams ||
                entry.bytes != controlBytes.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Commit control block ledger conflicts with its planned contribution"));
            foundControl = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::CommitReservationStorage) {
            if (foundReservation || entry.chargedToSocketBudget ||
                entry.itemCount != ledger.maximumBacklogDatagrams ||
                entry.bytes != reservationBytes.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Commit reservation storage differs from its planned contribution"));
            foundReservation = true;
        }
        if (entry.group == MediaRealtimeNetworkAccountingGroup::GlobalSequenceRecords) {
            if (foundGlobalRecords || entry.chargedToSocketBudget ||
                entry.itemCount != ledger.maximumBacklogDatagrams ||
                entry.bytes != globalRecords.value()) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Global sequence storage ledger conflicts with its actual fixed allocation"));
            }
            foundGlobalRecords = true;
        }
        if (entry.itemCount == 0 && entry.group !=
                MediaRealtimeNetworkAccountingGroup::EvidenceCorrelation &&
            entry.group != MediaRealtimeNetworkAccountingGroup::RtpProtocolRecords &&
            entry.group != MediaRealtimeNetworkAccountingGroup::RtpInstanceTable) {
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
    if ((raw ? !foundRawOwned : !foundMpegOwned) || !foundOutput || !foundReservation || !foundControl || !foundRtpRecords || !foundRtpTable)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Network ledger is missing RTP fixed storage"));
    if (!foundGlobalRecords) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Network ledger is missing global sequence record storage"));
    if (network != ledger.admittedNetworkBytes ||
        socket != ledger.admittedSocketBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "network resource ledger totals conflict with deployment budgets"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
