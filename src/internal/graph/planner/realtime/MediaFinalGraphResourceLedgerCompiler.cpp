#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
#include "internal/graph/runtime/network/MediaDatagramServiceScopeArbiter.h"
#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include "internal/graph/planner/realtime/MediaDatagramServiceScopePlanner.h"
#include "internal/graph/time/MediaClockDomainIdentity.h"
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

} // namespace

::media::Result<MediaFinalGraphResourceLedger>
compileLedger(
    const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
    std::span<const MediaNodeId> selectedNodes, bool credits)
{
    const auto selected = [&](MediaNodeId id) {
        return selectedNodes.empty() ||
            std::find(selectedNodes.begin(), selectedNodes.end(), id) != selectedNodes.end();
    };
    using Result = ::media::Result<MediaFinalGraphResourceLedger>;
    if (auto status = MediaRealtimeGraphResourceLedgerPlanner::validate(
            planningLedger); !status) {
        return Result::failure(status.error());
    }
    if (graph.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "final graph resource ledger requires the built graph"));
    }

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
        const auto reserveFixedStorage = [&](std::string owner, std::uint64_t bytes,
                                              std::uint64_t objects, std::string authority) -> ::media::Status {
            auto total = addTo(ledger.admittedGraphPayloadAndReservedStorageBytes,
                bytes, "shared service fixed runtime storage");
            if (!total) return ::media::Status::failure(total.error());
            ledger.admittedGraphPayloadAndReservedStorageBytes = total.value();
            ledger.entries.push_back({std::move(owner), {},
                MediaFinalGraphResourceScope::EngineManagedPayloadAndReservedStorage,
                bytes, 0, 0, objects, false, std::move(authority)});
            return ::media::Status::success();
        };
        // Root scope ownership stays with the shared execution domain after
        // the initial output is extracted. Dynamic suffixes only add members.
        const bool ownsRootService = selectedNodes.empty() || std::any_of(
            graph.nodes().begin(), graph.nodes().end(), [&](const MediaNode& node) {
                return selected(node.id) && node.kind == MediaNodeKind::VideoOutputFanout;
            });
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
                "MediaChannel embedded terminal MediaBufferRef sizeof"});
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
                0, coveredByGlobalPayloadLedger, std::move(authority)});
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
                "one shared MediaControlBuffer per draining execution segment sizeof"});
            ledger.entries.push_back(MediaFinalGraphResourceLedgerEntry{
                "branch-terminal-shared-control-block", {},
                MediaFinalGraphResourceScope::ObservedOnlyExternalAllocation,
                0, 0, 1, 0, false,
                "one std::make_shared control block; allocator bookkeeping size is implementation-owned"});
            ledger.outOfScopeAuthorities.push_back(
                "branch terminal std::make_shared control block allocator bookkeeping bytes");
        }

        for (const auto& node : graph.nodes()) {
            if (!selected(node.id)) continue;
            if (!productionRealtimeNode(node.kind)) {
                return Result::failure(::media::ErrorInfo::unsupported(
                    "final graph resource ledger has no retention contract for node: " +
                    node.name));
            }
            std::uint64_t retainedRefs = 1;
            std::string filterInputRetentionAuthority;
            std::uint64_t retainedVideoSurfaces = 0;
            if (node.kind == MediaNodeKind::VideoFrameRate &&
                !hasVideoFilter) {
                retainedVideoSurfaces = 2;
            } else if (node.kind == MediaNodeKind::VideoFilter) {
                retainedVideoSurfaces = 2;
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
                if (edge.from.nodeId != node.id && edge.to.nodeId != node.id) {
                    continue;
                }
                auto retained = Arithmetic::add(
                    retainedRefs,
                    static_cast<std::uint64_t>(edge.policy.queuePolicy.capacity),
                    "node port retention bound");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
            }
            if (node.kind == MediaNodeKind::VideoEncode) {
                auto retained = Arithmetic::add(
                    retainedRefs,
                    planningLedger.maximumEncoderRetainedFrames,
                    "opened encoder retained submissions");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
            } else if (node.kind == MediaNodeKind::VideoFilter) {
                auto retained = Arithmetic::add(
                    retainedRefs, 3U,
                    "video filter codec pending and prepared references");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
                auto execution = MediaVideoFilterExecutionPlanCodec::decode(node.options);
                if (!execution) return Result::failure(execution.error());
                if (execution.value().timing == MediaVideoFilterTimingAuthority::SourceFrame) {
                    const auto& retention = execution.value().output;
                    auto cached = Arithmetic::add(retainedRefs, retention.maximumRetainedInputFrames,
                        "source filter adapter retained input frames");
                    if (!cached) return Result::failure(cached.error());
                    retainedRefs = cached.value();
                    // These references retain the upstream source allocation;
                    // they are not allocations from the encoder output pool.
                    filterInputRetentionAuthority = retention.inputRetentionAuthority;
                }
            } else if (node.kind == MediaNodeKind::AudioEncode &&
                       planningLedger.media.audioUnits) {
                auto retained = Arithmetic::add(
                    retainedRefs,
                    static_cast<std::uint64_t>(
                        *planningLedger.media.audioUnits),
                    "audio encoder prepared residence frames");
                if (!retained) return Result::failure(retained.error());
                retainedRefs = retained.value();
            }
            std::uint64_t nodePayload = 0;
            if (node.kind == MediaNodeKind::RawRtpInput) {
                auto ingressBytes = optionUnsigned(
                    node, "rtp.ingress.batch_byte_capacity");
                auto descriptors = optionUnsigned(
                    node, "rtp.ingress.descriptor_capacity");
                if (!ingressBytes || !descriptors) {
                    return Result::failure(!ingressBytes
                        ? ingressBytes.error() : descriptors.error());
                }
                nodePayload = ingressBytes.value();
                retainedRefs = (std::max)(retainedRefs, descriptors.value());
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
                            : "+source-filter-input-cache:" + filterInputRetentionAuthority)});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "final graph resource ledger"));
    }

    const bool hasSelectedEncoder = std::any_of(graph.nodes().begin(), graph.nodes().end(),
        [&](const auto& node) { return selected(node.id) && node.kind == MediaNodeKind::VideoEncode; });
    if (planningLedger.hardwareEncoderSurfacePool && hasSelectedEncoder) {
        auto graphSurfaces = Arithmetic::add(
            videoFrameEdgeSurfaces,
            pipelinePendingSurfaces,
            "graph in-flight and pending hardware surfaces");
        auto pool = graphSurfaces
            ? Arithmetic::add(
            graphSurfaces.value(),
            planningLedger.maximumEncoderRetainedFrames,
            "encoder hardware frames initial pool")
            : graphSurfaces;
        if (!pool || pool.value() == 0) {
            return Result::failure(
                !pool ? pool.error() : ::media::ErrorInfo::notInitialized(
                    "encoder hardware frame pool is empty"));
        }
        ledger.encoderFramesPool = MediaEncoderHardwareFramesPoolPlan{
            pool.value(), videoFrameEdgeSurfaces,
            pipelinePendingSurfaces,
            planningLedger.maximumEncoderRetainedFrames,
            "final-encoder-input-edge+typed-upstream-pending+opened-encoder-retained-frames"};
        ledger.outOfScopeAuthorities.push_back(
            "device-and-driver-memory-is-out-of-scope-for-engine-managed-only");
    }
    if (planningLedger.hardwareMemory) {
        ledger.admittedDeviceAndDriverBytes =
            planningLedger.hardwareMemory->maximumDeviceAndDriverBytes;
    }
    if (ledger.admittedGraphPayloadAndReservedStorageBytes >
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
        graph, planningLedger, availablePayloadBytes,
        maximumPayloadObjects, selectedNodes);
    if (!payloadPlan) return Result::failure(payloadPlan.error());
    ledger.payloadCreditPlan = std::move(payloadPlan).value();
    ledger.videoPipelinePendingSurfaces = pipelinePendingSurfaces;
    return Result::success(std::move(ledger));
}

::media::Result<MediaFinalGraphResourceLedger> MediaFinalGraphResourceLedgerCompiler::compile(
    const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& planning,
    std::span<const MediaNodeId> nodes)
{
    return compileLedger(graph, planning, nodes, true);
}

::media::Result<MediaFinalGraphReferenceStoragePlan> MediaFinalGraphResourceLedgerCompiler::compileReferenceStorage(
    const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& planning,
    std::span<const MediaNodeId> nodes)
{
    using Result = ::media::Result<MediaFinalGraphReferenceStoragePlan>;
    if (nodes.empty()) return Result::failure(::media::ErrorInfo::invalidArgument("Reference segment is empty"));
    auto ledger = compileLedger(graph, planning, nodes, false);
    if (!ledger) return Result::failure(ledger.error());
    return Result::success({ledger.value().admittedGraphPayloadAndReservedStorageBytes,
        std::move(ledger.value().entries)});
}

} // namespace media::ffmpeg::graph
