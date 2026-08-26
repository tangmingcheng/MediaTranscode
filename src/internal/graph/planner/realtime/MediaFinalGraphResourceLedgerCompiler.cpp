#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"

#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
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
MediaFinalGraphResourceLedgerCompiler::compile(
    const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& planningLedger)
{
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
    std::uint64_t videoFrameEdgeSurfaces = 0;
    std::uint64_t pipelinePendingSurfaces = 0;
    const bool hasVideoFilter = std::any_of(
        graph.nodes().begin(), graph.nodes().end(), [](const MediaNode& node) {
            return node.kind == MediaNodeKind::VideoFilter;
        });

    try {
        ledger.entries.reserve(graph.edges().size() + graph.nodes().size());
        for (const auto& edge : graph.edges()) {
            if (!edge.isValid() || edge.payloadKind == MediaPayloadKind::Unknown) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "final graph resource ledger rejects an unknown edge"));
            }
            auto slots = queueSlotCount(edge);
            if (!slots) {
                return Result::failure(slots.error());
            }
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
            } else if (memory.enforceHardLimit && memory.maxBytes > 0) {
                payloadBytes = memory.maxBytes;
                authority += "+edge-buffer-hard-limit";
            } else if (networkLedgerPayload(edge.payloadKind)) {
                scope = MediaFinalGraphResourceScope::AccountedByNetworkLedger;
                authority = "typed-realtime-network-resource-ledger";
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

        for (const auto& node : graph.nodes()) {
            if (!productionRealtimeNode(node.kind)) {
                return Result::failure(::media::ErrorInfo::unsupported(
                    "final graph resource ledger has no retention contract for node: " +
                    node.name));
            }
            std::uint64_t retainedRefs = 1;
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
                    : "conservative-final-node-port-retention"});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "final graph resource ledger"));
    }

    if (planningLedger.hardwareEncoderSurfacePool) {
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
        maximumPayloadObjects, false);
    if (!payloadPlan) return Result::failure(payloadPlan.error());
    ledger.payloadCreditPlan = std::move(payloadPlan).value();
    return Result::success(std::move(ledger));
}

} // namespace media::ffmpeg::graph
