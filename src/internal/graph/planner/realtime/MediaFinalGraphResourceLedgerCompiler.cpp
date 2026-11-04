#include "internal/graph/planner/realtime/MediaNodeReferenceRetentionPlanner.h"
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"
#include "internal/graph/planner/realtime/MediaProtocolGenerationStoragePlanner.h"

#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerFactsPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmissionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/planner/realtime/MediaStartupBatchPayloadPlanner.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
#include "internal/graph/runtime/threading/MediaGraphThreadedExecutor.h"
#include "internal/graph/runtime/network/MediaDatagramServiceScopeArbiter.h"
#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include "internal/graph/planner/realtime/MediaDatagramServiceScopePlanner.h"
#include "internal/graph/time/MediaClockDomainIdentity.h"
#include "internal/graph/sync/lineage/MediaVideoLineageStagePreparation.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <new>
#include <optional>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using Arithmetic = MediaCheckedArithmetic;

bool productionRealtimeNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput:
    case MediaNodeKind::FileOutput:
    case MediaNodeKind::RealtimeInput:
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::StreamSplit:
    case MediaNodeKind::PacketFanout:
    case MediaNodeKind::FrameRoute:
    case MediaNodeKind::VideoOutputFanout:
    case MediaNodeKind::EncodedVideoOutputFanout:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::VideoTimestamp:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoPreparation:
    case MediaNodeKind::VideoFrameRate:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::CodecResolver:
    case MediaNodeKind::AudioCodecResolver:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::PacketSourceConfig:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::PacketStartGate:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::VideoOutputScheduler:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::DecodedVideoStartup:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::EncodedAudioCanonicalizer:
    case MediaNodeKind::ScheduledOutputRouter:
    case MediaNodeKind::ProjectMpegTsPlanSource:
    case MediaNodeKind::ScheduledTsAccessUnitAdapter:
    case MediaNodeKind::PacketMerge:
    case MediaNodeKind::FileMux:
    case MediaNodeKind::SdpWriter:
    case MediaNodeKind::RtpSdpPublisher:
    case MediaNodeKind::MpegTsRtpSdpPublisher:
    case MediaNodeKind::DatagramTransportPlanSource:
    case MediaNodeKind::RtpDatagramMaterializer:
    case MediaNodeKind::MpegTsDatagramMaterializer:
    case MediaNodeKind::DatagramShaper:
    case MediaNodeKind::ScheduledDatagramSender:
        return true;
    default:
        return false;
    }
}

bool externalLibraryPayload(MediaPayloadKind kind) noexcept
{
    switch (kind) {
    case MediaPayloadKind::FormatContext:
    case MediaPayloadKind::StreamDescriptor:
    case MediaPayloadKind::CodecContext:
    case MediaPayloadKind::CodecParameters:
    case MediaPayloadKind::Packet:
    case MediaPayloadKind::Frame:
    case MediaPayloadKind::OutputByteSink:
    case MediaPayloadKind::TsAccessUnit:
    case MediaPayloadKind::ProjectMpegTsRuntimePlan:
        return true;
    default:
        return false;
    }
}

bool networkLedgerPayload(MediaPayloadKind kind) noexcept
{
    switch (kind) {
    case MediaPayloadKind::ScheduledDatagramBatch:
    case MediaPayloadKind::WireDatagramBatch:
    case MediaPayloadKind::ScheduledWireDatagramBatch:
    case MediaPayloadKind::DatagramShapingPlan:
    case MediaPayloadKind::DatagramTransportPlan:
    case MediaPayloadKind::MpegTsProtocolDatagramBatch:
        return true;
    default:
        return false;
    }
}

bool globallyCreditedPayload(MediaPayloadKind kind) noexcept
{
    return kind == MediaPayloadKind::Packet ||
        kind == MediaPayloadKind::Frame ||
        kind == MediaPayloadKind::TsAccessUnit;
}

::media::Result<std::uint64_t> queueSlotCount(const MediaEdge& edge)
{
    const auto& queue = edge.policy.queuePolicy;
    if (!queue.bounded || queue.capacity == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "final graph resource ledger requires bounded edge queues"));
    }
    if (queue.mode == MediaQueueMode::SpscRing) {
        return Arithmetic::add(
            static_cast<std::uint64_t>(queue.capacity), 1U,
            "SPSC ring sentinel slot");
    }
    if (queue.mode == MediaQueueMode::Blocking &&
        queue.storageMode == MediaQueueStorageMode::AtomicPrepared) {
        return ::media::Result<std::uint64_t>::success(
            static_cast<std::uint64_t>(queue.capacity));
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "final graph resource ledger rejects queue storage without a preallocated slot bound"));
}

::media::Result<std::uint64_t> optionUnsigned(
    const MediaNode& node, const char* key)
{
    const std::string value = node.options.value(key);
    if (value.empty()) {
        return ::media::Result<std::uint64_t>::success(0);
    }
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("invalid final graph resource option: ") + key));
    }
    return ::media::Result<std::uint64_t>::success(parsed);
}

::media::Result<std::uint64_t> addTo(
    std::uint64_t total, std::uint64_t value, const char* fact)
{
    return Arithmetic::add(total, value, fact);
}

::media::Status admitPreparedStorage(const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& planning, MediaFinalGraphResourceAdmissionPlan& admission) try
{
    if (!planning.rawRtpPreparationStorage) return ::media::Status::success();
    std::vector<MediaNodeId> owners;
    for (const auto& node : graph.nodes())
        if (node.kind == MediaNodeKind::RawRtpInput) owners.push_back(node.id);
    auto storage = MediaPreparedRtpStorageAdmissionPlanner::plan(
        graph, *planning.rawRtpPreparationStorage, owners, 0);
    if (!storage) return ::media::Status::failure(storage.error());
    admission.fixedStorage.push_back(std::move(storage).value());
    return ::media::Status::success();
}
catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Prepared storage admission allocation failed"));
}

} // namespace

::media::Result<MediaFinalGraphResourceLedger>
compileLedger(
    const MediaGraph& graph,
    const MediaFinalGraphResourceAdmissionPlan& planningLedger,
    std::span<const MediaNodeId> selectedNodes, bool credits, bool enforceBudget = true)
{
    const auto selected = [&](MediaNodeId id) {
        return selectedNodes.empty() ||
            std::find(selectedNodes.begin(), selectedNodes.end(), id) != selectedNodes.end();
    };
    using Result = ::media::Result<MediaFinalGraphResourceLedger>;
    if (planningLedger.resourceScope == MediaRealtimeGraphResourceBudgetScope::Unknown ||
        ((credits || enforceBudget) && planningLedger.maximumGraphPayloadAndReservedStorageBytes == 0) ||
        planningLedger.maximumEncoderRetainedFrames == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument("Incomplete final graph admission facts"));
    }
    if (graph.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "final graph resource ledger requires the built graph"));
    }

    if (planningLedger.hardwareMemory) {
        if (auto status = planningLedger.hardwareMemory->validate(); !status)
            return Result::failure(status.error());
    }
    if (planningLedger.resourceScope == MediaRealtimeGraphResourceBudgetScope::EngineManagedPayloadAndReservedStoragePlusDevice &&
        !planningLedger.hardwareMemory)
        return Result::failure(::media::ErrorInfo::invalidArgument("Device admission lacks allocation evidence"));
    MediaFinalGraphResourceLedger ledger{
        planningLedger.resourceScope,
        planningLedger.maximumGraphPayloadAndReservedStorageBytes,
        0, 0, {}, {}, std::nullopt, {}};
    ledger.terminalReferenceSlots = 0;
    ledger.terminalControlObjects = 0;
    std::uint64_t videoFrameEdgeSurfaces = 0;
    std::uint64_t pipelinePendingSurfaces = 0;
    const bool hasVideoFilter = std::any_of(
        graph.nodes().begin(), graph.nodes().end(), [&](const MediaNode& node) {
            return selected(node.id) && node.kind == MediaNodeKind::VideoFilter;
        });

    try {
        ledger.entries.reserve(graph.edges().size() + graph.nodes().size());
        auto generationStorage = MediaProtocolGenerationStoragePlanner::plan(graph, selectedNodes);
        if (!generationStorage) return Result::failure(generationStorage.error());
        ledger.protocolGenerationStorage = std::move(generationStorage).value();
        for (const auto& instance : ledger.protocolGenerationStorage.instances) {
            const auto* owner = graph.findNode(instance.owner);
            if (!owner) return Result::failure(::media::ErrorInfo::invalidArgument(
                "Protocol generation storage lost its final graph owner"));
            ledger.entries.push_back({"protocol-generation-control:" + owner->name, {},
                MediaFinalGraphResourceScope::AccountedByNetworkLedger,
                instance.geometry.totalBytes, 0, 0, 3, false,
                "typed-fixed-network-control:shared-session-and-two-data-slots", {instance.owner}, {}, {}, {}, MediaFinalGraphReferenceKind::RetainingGroup});
        }
        for (const auto& storage : planningLedger.fixedStorage) {
            if(!storage.validReferenceIdentity() || storage.referenceNode || storage.referenceEdge)
                return Result::failure(::media::ErrorInfo::invalidArgument("Fixed storage must declare its retaining group, not an ordinary reference identity"));
            if (storage.storageAllocationOwner && storage.retentionNodes.empty())
                return Result::failure(::media::ErrorInfo::invalidArgument("Shared storage has no retaining node group"));
            std::size_t selectedOwners = 0;
            for (const auto id : storage.retentionNodes) {
                if (!graph.findNode(id) || std::count(storage.retentionNodes.begin(), storage.retentionNodes.end(), id) != 1)
                    return Result::failure(::media::ErrorInfo::invalidArgument("Fixed storage has missing or repeated retaining nodes"));
                if (selected(id)) ++selectedOwners;
            }
            if (!storage.retentionNodes.empty()) {
                if (!selectedOwners) continue;
                if (selectedOwners != storage.retentionNodes.size())
                    return Result::failure(::media::ErrorInfo::invalidArgument("Selected nodes split a shared storage retaining group"));
            }
            if (storage.owner.empty() || storage.authority.empty() || storage.payloadBytes == 0 ||
                storage.coveredByGlobalPayloadLedger || storage.scope !=
                    MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage ||
                std::any_of(ledger.entries.begin(), ledger.entries.end(), [&](const auto& entry) {
                    return entry.owner == storage.owner ||
                        (storage.storageAllocationOwner && entry.storageAllocationOwner == storage.storageAllocationOwner);
                }))
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Invalid or duplicate final graph fixed storage owner"));
            auto total = Arithmetic::add(ledger.admittedGraphPayloadAndReservedStorageBytes,
                storage.payloadBytes, "prepared graph fixed storage");
            if (!total) return Result::failure(total.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = total.value();
            ledger.entries.push_back(storage);
        }
        const auto reserveFixedStorage = [&](std::string owner, std::uint64_t bytes,
                                              std::uint64_t objects, std::string authority) -> ::media::Status {
            auto total = addTo(ledger.admittedGraphPayloadAndReservedStorageBytes,
                bytes, "shared service fixed runtime storage");
            if (!total) return ::media::Status::failure(total.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = total.value();
            ledger.entries.push_back({std::move(owner), {},
                MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
                bytes, 0, 0, objects, false, std::move(authority), {}, {}, {}, {}, MediaFinalGraphReferenceKind::None});
            return ::media::Status::success();
        };
        // Root scope ownership stays with the shared execution domain after
        // the initial output is extracted. Dynamic suffixes only add members.
        const bool ownsRootService = selectedNodes.empty() || std::any_of(
            graph.nodes().begin(), graph.nodes().end(), [&](const MediaNode& node) {
                return selected(node.id) && node.kind == MediaNodeKind::VideoOutputFanout;
            });
        // The root context was compiled with every initial node. Extraction
        // erases logical members, never its backing. Every detached context
        // allocates its own exact contracts, charged concurrently to its lease.
        const auto memberCount = static_cast<std::uint64_t>(selectedNodes.empty()
            ? graph.nodeCount() : selectedNodes.size());
        const auto contractCount = static_cast<std::uint64_t>(ownsRootService
            ? graph.nodeCount() : memberCount);
        auto contracts = Arithmetic::multiply(contractCount, sizeof(MediaWorkerExecutionPlan),
            "exact compiled worker contract backing including retained root allocation");
        if (!contracts) return Result::failure(contracts.error());
        if (auto reserved = reserveFixedStorage("compiled-worker-contracts", contracts.value(), contractCount,
                "MediaFixedCapacityStorage exact backing; root retains initial graph count after detach"); !reserved)
            return Result::failure(reserved.error());
        auto workers = Arithmetic::multiply(memberCount,
            sizeof(MediaGraphWorker) + sizeof(std::unique_ptr<MediaGraphWorker>) + sizeof(MediaNodeId),
            "worker objects plus exact owner slots and attempted configure identity backing");
        if (!workers) return Result::failure(workers.error());
        if (auto reserved = reserveFixedStorage("execution-worker-owners", workers.value(), memberCount,
                "one worker object+fixed owner slot+fixed scheduler rollback identity per executing node"); !reserved)
            return Result::failure(reserved.error());
        if (ownsRootService) {
            // Branch context/scheduler/gate are already embedded in the branch
            // fixedStorageBytes charge below; only the root owns these objects.
            if (auto reserved = reserveFixedStorage("root-execution-controllers",
                    sizeof(MediaGraphExecutionContext) + sizeof(MediaGraphScheduler) + sizeof(MediaGraphThreadedExecutor), 3,
                    "root context+scheduler+executor object sizeof including gate, mutexes and container descriptors"); !reserved)
                return Result::failure(reserved.error());
        }
        ledger.outOfScopeAuthorities.push_back(
            "native worker/probe thread stacks, MMCSS internal resources, allocator bookkeeping and diagnostic string storage");
        if (ownsRootService) {
            std::map<std::pair<MediaDatagramServiceScopeKind, std::string>,
                MediaDatagramServiceScopeContract> scopes;
            bool videoClock = false;
            for (const auto& node : graph.nodes()) {
                videoClock = videoClock || node.kind == MediaNodeKind::VideoOutputScheduler;
                if (node.kind != MediaNodeKind::DatagramTransportPlanSource) continue;
                auto transport = MediaDatagramTransportPlanSourceNodePlanCodec::decode(node);
                if (!transport) return Result::failure(transport.error());
                auto scope = MediaDatagramServiceScopePlanner::plan(transport.value());
                if (!scope) return Result::failure(scope.error());
                const auto key = std::pair{scope.value().kind, scope.value().scopeId};
                const auto [found, inserted] = scopes.emplace(key, scope.value());
                if (!inserted && found->second != scope.value()) return Result::failure(
                    ::media::ErrorInfo::invalidArgument("Shared service scope has conflicting prepared contracts"));
            }
            for (const auto& entry : scopes) {
                if (auto reserved = reserveFixedStorage("service-scope:" + entry.first.second,
                        sizeof(MediaDatagramServiceScopeArbiter), 1,
                        "one persistent service-scope arbiter object sizeof"); !reserved)
                    return Result::failure(reserved.error());
            }
            if (videoClock && !scopes.empty()) {
                if (auto reserved = reserveFixedStorage("service-clock-domain",
                        sizeof(MediaSteadyClockDomain), 1,
                        "one shared video steady clock anchor object sizeof"); !reserved)
                    return Result::failure(reserved.error());
            }
            if (!scopes.empty()) ledger.outOfScopeAuthorities.push_back(
                "service scope strings and shared ownership allocator/control-block bookkeeping bytes");
        }
        for (const auto& node : graph.nodes()) {
            if (!selected(node.id) || node.kind != MediaNodeKind::ScheduledDatagramSender) continue;
            if (auto reserved = reserveFixedStorage("service-member:" + node.name,
                    sizeof(MediaDatagramServiceScopeArbiter::Member) +
                        sizeof(MediaDatagramServiceScopeArbiter::SubmitGuard), 2,
                    "one intrusive member and at most one live submit guard per sender sizeof"); !reserved)
                return Result::failure(reserved.error());
            ledger.outOfScopeAuthorities.push_back(
                "service member shared ownership allocator/control-block bookkeeping bytes");
        }

        for (const auto& node : graph.nodes()) {
            if (!selected(node.id)) continue;
            std::string_view identity;
            switch (node.kind) {
            case MediaNodeKind::VideoDecode: identity = VideoDecodeNode::generationPurgeIdentity(); break;
            case MediaNodeKind::VideoFilter: identity = VideoFilterNode::generationPurgeIdentity(); break;
            case MediaNodeKind::VideoEncode: identity = VideoEncodeNode::generationPurgeIdentity(); break;
            default: continue;
            }
            auto capacity = prepareMediaVideoLineageStageCapacity(node, identity);
            if (!capacity) return Result::failure(capacity.error());
            if (!capacity.value()) continue; // Runtime factory creates no registry in this case.
            auto storage = MediaCodecLineageRegistry::runtimeStorage(*capacity.value());
            if (!storage) return Result::failure(storage.error());
            if (auto reserved = reserveFixedStorage("lineage-registry:" + std::to_string(node.id.value),
                    storage.value().totalBytes, *capacity.value(),
                    "actual runtime registry/state control blocks, map nodes and slot-owned engine opaque allocations"); !reserved)
                return Result::failure(reserved.error());
            auto& entry = ledger.entries.back();
            entry.referenceKind = MediaFinalGraphReferenceKind::RetainingGroup;
            entry.retentionNodes = {node.id};
        }

        for (const auto& edge : graph.edges()) {
            if (!selected(edge.to.nodeId)) continue;
            if (!edge.isValid() || edge.payloadKind == MediaPayloadKind::Unknown) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "final graph resource ledger rejects an unknown edge"));
            }
            auto slots = queueSlotCount(edge);
            if (!slots) {
                return Result::failure(slots.error());
            }
            auto referenceBytes = Arithmetic::multiply(slots.value(), sizeof(MediaBufferRef),
                "exact edge reference ring backing");
            if (!referenceBytes) return Result::failure(referenceBytes.error());
            if (auto reserved = reserveFixedStorage("queue-storage:" + edge.name,
                    referenceBytes.value(), slots.value(), "exact ring element backing sizeof(MediaBufferRef)"); !reserved)
                return Result::failure(reserved.error());
            if (edge.policy.queuePolicy.mode == MediaQueueMode::Blocking &&
                edge.policy.queuePolicy.storageMode == MediaQueueStorageMode::AtomicPrepared) {
                auto preparedBytes = Arithmetic::multiply(edge.policy.queuePolicy.capacity,
                    sizeof(MediaBufferRef), "bounded prepared output reference backing");
                if (!preparedBytes) return Result::failure(preparedBytes.error());
                // Capacity reservations bound all live admitted batches on an
                // edge together. Its single producer can additionally prepare
                // one candidate transaction before acquiring capacity.
                for (const auto* owner : {"reserved-output-storage:", "candidate-output-storage:"}) {
                    if (auto reserved = reserveFixedStorage(std::string(owner) + edge.name,
                            preparedBytes.value(), edge.policy.queuePolicy.capacity,
                            "exact prepared batch backing+edge capacity+single producer execution"); !reserved)
                        return Result::failure(reserved.error());
                }
            }
            // MediaChannel embeds one terminal reference even when this edge
            // never receives a branch EOS. The allocation belongs to its
            // consuming execution segment, exactly once across partitions.
            auto terminalStorage = addTo(
                ledger.admittedGraphPayloadAndReservedStorageBytes,
                sizeof(MediaBufferRef), "channel terminal reference storage");
            if (!terminalStorage) return Result::failure(terminalStorage.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = terminalStorage.value();
            ++ledger.terminalReferenceSlots;
            if (!selectedNodes.empty() && !selected(edge.from.nodeId) &&
                (edge.payloadKind == MediaPayloadKind::Frame ||
                 edge.payloadKind == MediaPayloadKind::Packet)) {
                ledger.terminalControlObjects = 1;
            }
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "terminal-cell:" + edge.name, {},
                MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
                sizeof(MediaBufferRef), 1, 0, 1, false,
                "MediaChannel embedded terminal MediaBufferRef sizeof", {}, {}, {}, {}, MediaFinalGraphReferenceKind::None});
            std::uint64_t payloadBytes = 0;
            bool coveredByGlobalPayloadLedger = false;
            MediaFinalGraphResourceScope scope =
                MediaFinalGraphResourceScope::
                    EngineManagedPayloadAndReservedStorage;
            std::string authority =
                "final-edge-bounded-queue-slot-count";
            const auto& memory = edge.policy.bufferPolicy.memoryBudget;
            if (globallyCreditedPayload(edge.payloadKind)) {
                coveredByGlobalPayloadLedger = true;
                authority += "+global-payload-credit-ledger";
            } else if (networkLedgerPayload(edge.payloadKind)) {
                scope = MediaFinalGraphResourceScope::AccountedByNetworkLedger;
                authority = "typed-realtime-network-resource-ledger";
            } else if (memory.enforceHardLimit && memory.maxBytes > 0) {
                payloadBytes = memory.maxBytes;
                authority += "+edge-buffer-hard-limit";
            } else if (externalLibraryPayload(edge.payloadKind)) {
                scope = MediaFinalGraphResourceScope::
                    ObservedOnlyExternalAllocation;
                authority = "ffmpeg-owned-media-allocation";
            }
            if (payloadBytes > 0) {
                auto total = addTo(
                    ledger.admittedGraphPayloadAndReservedStorageBytes,
                    payloadBytes,
                    "final graph independent edge payload");
                if (!total) return Result::failure(total.error());
                ledger.admittedGraphPayloadAndReservedStorageBytes =
                    total.value();
            }
            if (edge.streamKind == MediaStreamKind::Video &&
                edge.payloadKind == MediaPayloadKind::Frame) {
                const auto target = std::find_if(
                    graph.nodes().begin(), graph.nodes().end(),
                    [&](const MediaNode& node) {
                        return node.id == edge.to.nodeId;
                    });
                if (target != graph.nodes().end() &&
                    target->kind == MediaNodeKind::VideoEncode) {
                    auto surfaces = Arithmetic::add(
                        videoFrameEdgeSurfaces,
                        static_cast<std::uint64_t>(
                            edge.policy.queuePolicy.capacity),
                        "encoder input edge in-flight surfaces");
                    if (!surfaces) return Result::failure(surfaces.error());
                    videoFrameEdgeSurfaces = surfaces.value();
                }
            }
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "edge:" + edge.name,
                edge.policy.bufferPolicy.sharedAllocationGroup,
                scope, payloadBytes, slots.value(),
                static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity),
                0, coveredByGlobalPayloadLedger, std::move(authority), {}, {}, std::nullopt, edge.id, MediaFinalGraphReferenceKind::Edge});
        }

        if (ledger.terminalControlObjects != 0) {
            if (auto reserved = reserveFixedStorage("segment-reclamation-owner",
                    MediaRuntimeBranch::fixedStorageBytes(), 2,
                    "one runtime segment and one single-shot reclamation owner sizeof"); !reserved)
                return Result::failure(reserved.error());
            ledger.outOfScopeAuthorities.push_back(
                "native reclamation thread stack and STL thread/control-block allocations");
            auto controlStorage = addTo(
                ledger.admittedGraphPayloadAndReservedStorageBytes,
                sizeof(MediaControlBuffer), "branch shared terminal control object");
            if (!controlStorage) return Result::failure(controlStorage.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = controlStorage.value();
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "branch-terminal-control", {},
                MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
                sizeof(MediaControlBuffer), 0, 1, 0, false,
                "one shared MediaControlBuffer per draining execution segment sizeof", {}, {}, {}, {}, MediaFinalGraphReferenceKind::None});
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "branch-terminal-shared-control-block", {},
                MediaFinalGraphResourceScope::ObservedOnlyExternalAllocation,
                0, 0, 1, 0, false,
                "one std::make_shared control block; allocator bookkeeping size is implementation-owned", {}, {}, {}, {}, MediaFinalGraphReferenceKind::None});
            ledger.outOfScopeAuthorities.push_back(
                "branch terminal std::make_shared control block allocator bookkeeping bytes");
        }

        for (const auto& node : graph.nodes()) {
            if (!selected(node.id)) continue;
            const auto preparedStorage = std::find_if(planningLedger.fixedStorage.begin(),
                planningLedger.fixedStorage.end(), [&](const auto& storage) {
                    return (node.kind == MediaNodeKind::AvContinuousAggregate || node.kind == MediaNodeKind::ContinuousAudioAggregate) &&
                        std::find(storage.retentionNodes.begin(),storage.retentionNodes.end(),node.id)!=storage.retentionNodes.end();
                });
            if ((node.kind == MediaNodeKind::AvContinuousAggregate || node.kind == MediaNodeKind::ContinuousAudioAggregate) &&
                preparedStorage != planningLedger.fixedStorage.end() &&
                preparedStorage->maximumBufferObjects > 0) continue;
            if (!productionRealtimeNode(node.kind)) {
                return Result::failure(::media::ErrorInfo::unsupported(
                    "final graph resource ledger has no retention contract for node: " +
                    node.name));
            }
            std::vector<std::uint64_t> incidentCapacities;
            std::string filterInputRetentionAuthority;
            std::uint64_t retainedVideoSurfaces = 0;
            if (node.kind == MediaNodeKind::VideoFrameRate &&
                !hasVideoFilter) {
                retainedVideoSurfaces = 2;
            } else if (node.kind == MediaNodeKind::VideoFilter) {
                retainedVideoSurfaces = 1;
            } else if (node.kind == MediaNodeKind::VideoPreparation) {
                // Pending input is moved into the reservation, not copied pixels.
                retainedVideoSurfaces = 1;
            } else if (node.kind == MediaNodeKind::VideoEncode) {
                retainedVideoSurfaces = 1;
            }
            if (retainedVideoSurfaces > 0) {
                auto pending = Arithmetic::add(
                    pipelinePendingSurfaces, retainedVideoSurfaces,
                    "video pipeline pending surfaces");
                if (!pending) return Result::failure(pending.error());
                pipelinePendingSurfaces = pending.value();
            }
            for (const auto& edge : graph.edges()) {
                if (edge.from.nodeId == node.id || edge.to.nodeId == node.id)
                    incidentCapacities.push_back(static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity));
            }
            std::optional<std::uint64_t> acquiringCapacity, descriptorCapacity, filterRetainedInputs;
            if (node.kind == MediaNodeKind::RtpPacketClockBinder) {
                auto acquiring = optionUnsigned(node, "rtp_clock_binder.acquiring_capacity");
                if (!acquiring) return Result::failure(acquiring.error());
                acquiringCapacity = acquiring.value();
            }
            if (node.kind == MediaNodeKind::RawRtpInput) {
                auto descriptors = optionUnsigned(node, "rtp.ingress.descriptor_capacity");
                if (!descriptors) return Result::failure(descriptors.error());
                descriptorCapacity = descriptors.value();
            }
            if (node.kind == MediaNodeKind::VideoFilter) {
                auto execution = MediaVideoFilterExecutionPlanCodec::decode(node.options);
                if (!execution) return Result::failure(execution.error());
                const bool source = execution.value().timing == MediaVideoFilterTimingAuthority::SourceFrame;
                filterRetainedInputs = source ? execution.value().output.maximumRetainedInputFrames : 0;
                if (source) filterInputRetentionAuthority = execution.value().output.inputRetentionAuthority;
            }
            auto nodeRetention = MediaNodeReferenceRetentionPlanner::plan({node.kind, incidentCapacities,
                acquiringCapacity, descriptorCapacity, filterRetainedInputs});
            if (!nodeRetention) return Result::failure(nodeRetention.error());
            auto retainedRefs = nodeRetention.value();
            if (node.kind == MediaNodeKind::AvStartupCoordinator) {
                auto payload = MediaStartupBatchPayloadPlanner::plan(node);
                if (!payload) return Result::failure(payload.error());
                auto events = MediaStartupEventMediaPlanner::plan(graph, payload.value());
                if (!events) return Result::failure(events.error());
                for (const auto& event : events.value()) {
                    const auto* edge = graph.findEdge(event.edge);
                    if (!edge || !selected(edge->from.nodeId) || !selected(edge->to.nodeId) ||
                        std::any_of(ledger.startupEventMedia.begin(), ledger.startupEventMedia.end(),
                            [&](const auto& existing) { return existing.edge == event.edge; }))
                        return Result::failure(::media::ErrorInfo::invalidArgument(
                            "Startup media routing has split or repeated ownership"));
                    ledger.startupEventMedia.push_back(event);
                }
                auto nodes = MediaStartupNodeMediaPlanner::plan(graph, payload.value(), events.value());
                if (!nodes) return Result::failure(nodes.error());
                for (const auto& bound : nodes.value()) {
                    if (!selected(bound.node) || std::any_of(ledger.startupNodeMedia.begin(), ledger.startupNodeMedia.end(),
                            [&](const auto& existing) { return existing.node == bound.node; }))
                        return Result::failure(::media::ErrorInfo::invalidArgument(
                            "Startup node media has split or repeated ownership"));
                    ledger.startupNodeMedia.push_back(bound);
                }
                auto batch = Arithmetic::add(payload.value().videoUnits,
                    payload.value().audioPacketUnits.value_or(0),
                    "startup batch payload object retention");
                auto retained = batch ? Arithmetic::add(retainedRefs, batch.value(),
                    "startup store and release-batch object credits") : batch;
                if (!retained) return Result::failure(retained.error());
                // Leases follow the batch through GraphEvent wrappers and the
                // release extractor; reference copies do not allocate new AUs.
                retainedRefs = retained.value();
            }
            if (node.kind == MediaNodeKind::VideoEncode) {
                auto retained = Arithmetic::add(
                    retainedRefs,
                    planningLedger.maximumEncoderRetainedFrames,
                    "opened encoder retained submissions");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
            } else if (node.kind == MediaNodeKind::AudioEncode &&
                       planningLedger.maximumAudioEncoderRetainedFrames) {
                auto retained = Arithmetic::add(
                    retainedRefs,
                    static_cast<std::uint64_t>(
                        *planningLedger.maximumAudioEncoderRetainedFrames),
                    "audio encoder prepared residence frames");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
            }
            std::uint64_t nodePayload = 0;
            if (node.kind == MediaNodeKind::RawRtpInput) {
                auto ingressBytes = optionUnsigned(
                    node, "rtp.ingress.batch_byte_capacity");
                if (!ingressBytes) return Result::failure(ingressBytes.error());
                nodePayload = ingressBytes.value();
            }
            auto total = addTo(
                ledger.admittedGraphPayloadAndReservedStorageBytes,
                nodePayload, "final graph node reserved payload");
            if (!total) return Result::failure(total.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = total.value();
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "node:" + node.name, {},
                MediaFinalGraphResourceScope::
                    EngineManagedPayloadAndReservedStorage,
                nodePayload, 0, retainedRefs, retainedRefs,
                false,
                node.kind == MediaNodeKind::RawRtpInput
                    ? "final-node-port-retention+planned-raw-rtp-ingress-arena"
                    : "conservative-final-node-port-retention" +
                        (filterInputRetentionAuthority.empty() ? std::string{}
                            : "+source-filter-input-cache:" + filterInputRetentionAuthority), {}, {}, node.id, std::nullopt, MediaFinalGraphReferenceKind::Node});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "final graph resource ledger"));
    }

    for(const auto& entry:ledger.entries)if(!entry.validReferenceIdentity())
        return Result::failure(::media::ErrorInfo::invalidArgument("Final graph storage lacks an explicit reference identity domain"));

    const bool hasSelectedEncoder = std::any_of(graph.nodes().begin(), graph.nodes().end(),
        [&](const auto& node) { return selected(node.id) && node.kind == MediaNodeKind::VideoEncode; });
    if (planningLedger.encoderFramesPool && hasSelectedEncoder) {
        auto pool = MediaEncoderHardwareFramesPoolPlanner::plan(*planningLedger.encoderFramesPool);
        if (!pool) return Result::failure(pool.error());
        if (pool.value() != *planningLedger.encoderFramesPool)
            return Result::failure(::media::ErrorInfo::invalidArgument("Frozen encoder pool differs from retention facts"));
        ledger.encoderFramesPool = planningLedger.encoderFramesPool;
        ledger.outOfScopeAuthorities.push_back(
            "device-and-driver-memory-is-out-of-scope-for-engine-managed-only");
    } else if (planningLedger.hardwareEncoderSurfacePool && hasSelectedEncoder) {
        auto pool = MediaEncoderHardwareFramesPoolPlanner::plan({videoFrameEdgeSurfaces,
            pipelinePendingSurfaces, planningLedger.maximumEncoderRetainedFrames, 0,
            "final-encoder-input-edge+typed-upstream-pending+opened-encoder-retained-frames"});
        if (!pool) return Result::failure(pool.error());
        ledger.encoderFramesPool = std::move(pool).value();
        ledger.outOfScopeAuthorities.push_back(
            "device-and-driver-memory-is-out-of-scope-for-engine-managed-only");
    }
    if (planningLedger.hardwareMemory) {
        ledger.admittedDeviceAndDriverBytes =
            planningLedger.hardwareMemory->maximumDeviceAndDriverBytes;
    }
    if (enforceBudget && ledger.admittedGraphPayloadAndReservedStorageBytes >
        ledger.maximumGraphPayloadAndReservedStorageBytes) {
        const auto maximumPayload = std::max_element(
            ledger.entries.begin(), ledger.entries.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.payloadBytes < rhs.payloadBytes;
            });
        const auto maximumObjects = std::max_element(
            ledger.entries.begin(), ledger.entries.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.maximumBufferObjects < rhs.maximumBufferObjects;
            });
        std::ostringstream message;
        message
            << "engine-managed graph memory budget cannot admit the final DAG retention ledger"
            << " required_payload_and_reserved_bytes="
            << ledger.admittedGraphPayloadAndReservedStorageBytes
            << " budget_payload_and_reserved_bytes="
            << ledger.maximumGraphPayloadAndReservedStorageBytes
            << " entries=" << ledger.entries.size();
        if (maximumPayload != ledger.entries.end()) {
            message << " maximum_payload_owner=" << maximumPayload->owner
                    << " maximum_payload_bytes="
                    << maximumPayload->payloadBytes
                    << " maximum_payload_authority="
                    << maximumPayload->authority;
        }
        if (maximumObjects != ledger.entries.end()) {
            message << " maximum_objects_owner=" << maximumObjects->owner
                    << " maximum_buffer_objects="
                    << maximumObjects->maximumBufferObjects
                    << " maximum_objects_authority="
                    << maximumObjects->authority;
        }
        return Result::failure(::media::ErrorInfo::invalidArgument(
            message.str()));
    }
    if (!credits) {
        ledger.videoPipelinePendingSurfaces = pipelinePendingSurfaces;
        return Result::success(std::move(ledger));
    }
    std::uint64_t maximumPayloadObjects = 0;
    for (const auto& entry : ledger.entries) {
        auto objects = Arithmetic::add(
            maximumPayloadObjects, entry.maximumBufferObjects,
            "final DAG payload object credits");
        if (!objects) return Result::failure(objects.error());
        maximumPayloadObjects = objects.value();
    }
    const std::uint64_t availablePayloadBytes =
        ledger.maximumGraphPayloadAndReservedStorageBytes -
        ledger.admittedGraphPayloadAndReservedStorageBytes;
    auto payloadPlan = MediaGraphPayloadProducerRegistryCompiler::compile(
        graph, planningLedger.producers, availablePayloadBytes,
        maximumPayloadObjects, selectedNodes);
    if (!payloadPlan) return Result::failure(payloadPlan.error());
    ledger.payloadCreditPlan = std::move(payloadPlan).value();
    if (!planningLedger.accounts.empty()) {
        if (planningLedger.producerAccounts.size() != planningLedger.producers.size() ||
            planningLedger.minimumProgressObjects.size() != planningLedger.producers.size() ||
            ledger.payloadCreditPlan.producers.size() != planningLedger.producers.size())
            return Result::failure(::media::ErrorInfo::invalidArgument("Final producer account coverage differs"));
        std::uint64_t bytes = 0;
        std::uint64_t objects = 0;
        for (const auto& account : planningLedger.accounts) {
            auto nextBytes = Arithmetic::add(bytes, account.maximumBytes, "final allocation accounts bytes");
            auto nextObjects = Arithmetic::add(objects, account.maximumObjects, "final allocation accounts objects");
            if (!nextBytes || !nextObjects) return Result::failure(!nextBytes ? nextBytes.error() : nextObjects.error());
            bytes = nextBytes.value();
            objects = nextObjects.value();
        }
        if (bytes > availablePayloadBytes)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Frozen allocation account budgets exceed the final graph payload allowance"));
        ledger.payloadCreditPlan.maximumBytes = bytes;
        ledger.payloadCreditPlan.maximumObjects = objects;
        ledger.payloadCreditPlan.accounts = planningLedger.accounts;
        for (std::size_t i = 0; i < planningLedger.producers.size(); ++i) {
            ledger.payloadCreditPlan.producers[i].accountId = planningLedger.producerAccounts[i];
            const auto& producer = ledger.payloadCreditPlan.producers[i];
            const auto account = std::find_if(planningLedger.accounts.begin(), planningLedger.accounts.end(),
                [&](const auto& value) { return value.accountId == producer.accountId; });
            const auto minimum = planningLedger.minimumProgressObjects[i];
            auto minimumBytes = Arithmetic::multiply(minimum, producer.maximumReservedBytes(),
                "producer minimum forward-progress allocation bytes");
            if (!minimumBytes) return Result::failure(minimumBytes.error());
            if (!minimum || account == planningLedger.accounts.end() || account->maximumObjects < minimum ||
                account->maximumBytes < minimumBytes.value())
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Independent producer budget cannot admit its codec and pipeline progress working set"));
        }
        if (!ledger.payloadCreditPlan.isCompleteAndValid())
            return Result::failure(::media::ErrorInfo::invalidArgument("Final allocation account plan is incomplete"));
    }
    ledger.videoPipelinePendingSurfaces = pipelinePendingSurfaces;
    return Result::success(std::move(ledger));
}

::media::Result<MediaFinalGraphResourceLedger> MediaFinalGraphResourceLedgerCompiler::compile(
    const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& planning,
    std::span<const MediaNodeId> nodes)
{
    using Result = ::media::Result<MediaFinalGraphResourceLedger>;
    if (auto status = MediaRealtimeGraphResourceLedgerPlanner::validate(planning); !status)
        return Result::failure(status.error());
    auto producers = MediaGraphPayloadProducerFactsPlanner::plan(graph, planning, nodes);
    if (!producers) return Result::failure(producers.error());
    MediaFinalGraphResourceAdmissionPlan admission{planning.resourceScope,
        planning.maximumGraphPayloadAndReservedStorageBytes, planning.maximumEncoderRetainedFrames,
        planning.media.audioUnits, planning.hardwareMemory, planning.hardwareEncoderSurfacePool,
        std::nullopt, std::move(producers).value(), {}};
    if (auto status = admitPreparedStorage(graph, planning, admission); !status)
        return Result::failure(status.error());
    return compileLedger(graph, admission, nodes, true);
}

::media::Result<MediaFinalGraphResourceLedger> MediaFinalGraphResourceLedgerCompiler::compile(
    const MediaGraph& graph, const MediaFinalGraphResourceAdmissionPlan& admission)
{
    return compileLedger(graph, admission, {}, true);
}

::media::Result<MediaFinalGraphReferenceStoragePlan> MediaFinalGraphResourceLedgerCompiler::compileReferenceStorage(
    const MediaGraph& graph, const MediaFinalGraphResourceAdmissionPlan& facts)
{
    using Result = ::media::Result<MediaFinalGraphReferenceStoragePlan>;
    auto ledger = compileLedger(graph, facts, {}, false, false);
    if (!ledger) return Result::failure(ledger.error());
    return Result::success({ledger.value().admittedGraphPayloadAndReservedStorageBytes,
        std::move(ledger.value().entries), std::move(ledger.value().startupEventMedia), std::move(ledger.value().startupNodeMedia),
        std::move(ledger.value().protocolGenerationStorage)});
}

::media::Result<MediaFinalGraphReferenceStoragePlan> MediaFinalGraphResourceLedgerCompiler::compileReferenceStorage(
    const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& planning,
    std::span<const MediaNodeId> nodes)
{
    using Result = ::media::Result<MediaFinalGraphReferenceStoragePlan>;
    if (nodes.empty()) return Result::failure(::media::ErrorInfo::invalidArgument("Reference segment is empty"));
    if (auto status = MediaRealtimeGraphResourceLedgerPlanner::validate(planning); !status)
        return Result::failure(status.error());
    MediaFinalGraphResourceAdmissionPlan admission{planning.resourceScope,
        planning.maximumGraphPayloadAndReservedStorageBytes, planning.maximumEncoderRetainedFrames,
        planning.media.audioUnits, planning.hardwareMemory, planning.hardwareEncoderSurfacePool,
        std::nullopt, {}, {}};
    if (auto status = admitPreparedStorage(graph, planning, admission); !status)
        return Result::failure(status.error());
    auto ledger = compileLedger(graph, admission, nodes, false);
    if (!ledger) return Result::failure(ledger.error());
    return Result::success({ledger.value().admittedGraphPayloadAndReservedStorageBytes,
        std::move(ledger.value().entries), std::move(ledger.value().startupEventMedia), std::move(ledger.value().startupNodeMedia),
        std::move(ledger.value().protocolGenerationStorage)});
}

} // namespace media::ffmpeg::graph
