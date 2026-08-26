#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"

#include <algorithm>
#include <new>

namespace media::ffmpeg::graph {
namespace {

bool allocatesPayload(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
        return true;
    default:
        return false;
    }
}

bool runtimeIntegrated(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::AudioEncode:
        return true;
    default:
        return false;
    }
}

::media::Result<std::uint64_t> maximumBytes(
    MediaNodeKind producerKind,
    const MediaEdge& edge,
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (producerKind != MediaNodeKind::VideoEncode &&
        producerKind != MediaNodeKind::AudioEncode) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "producer registry lacks an authoritative source allocation bound"));
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Video) {
        return ::media::Result<std::uint64_t>::success(
            ledger.media.videoUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Audio &&
        ledger.media.audioUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.media.audioUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Video) {
        return ::media::Result<std::uint64_t>::success(
            ledger.videoSurfaceUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Audio &&
        ledger.audioFrameUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.audioFrameUnitBytes);
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "producer registry lacks a prepared payload bound"));
}

bool sameKey(
    const MediaGraphPayloadProducerStrategy& strategy,
    const MediaEdge& edge) noexcept
{
    return strategy.nodeId == edge.from.nodeId &&
        strategy.streamKind == edge.streamKind &&
        strategy.payloadKind == edge.payloadKind;
}

bool sameKey(
    const MediaGraphPayloadProducerRequirement& requirement,
    const MediaEdge& edge) noexcept
{
    return requirement.nodeId == edge.from.nodeId &&
        requirement.streamKind == edge.streamKind &&
        requirement.payloadKind == edge.payloadKind;
}

} // namespace

::media::Result<MediaGraphPayloadCreditPlan>
MediaGraphPayloadProducerRegistryCompiler::compile(
    const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
    std::uint64_t availablePayloadBytes,
    std::uint64_t maximumPayloadObjects)
{
    using Result = ::media::Result<MediaGraphPayloadCreditPlan>;
    if (availablePayloadBytes == 0 || maximumPayloadObjects == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "payload producer registry requires positive global credits"));
    }
    MediaGraphPayloadCreditPlan plan;
    plan.maximumBytes = availablePayloadBytes;
    plan.maximumObjects = maximumPayloadObjects;
    plan.producerStrategyVersion = 1;
    plan.integration = MediaGraphPayloadCreditIntegration::Complete;
    plan.authority =
        "final-dag-producer-registry+prepared-emission+global-payload-budget";
    try {
        for (const auto& node : graph.nodes()) {
            if (!allocatesPayload(node.kind)) continue;
            bool foundProducerEdge = false;
            for (const auto& edge : graph.edges()) {
                if (edge.from.nodeId != node.id ||
                    (edge.payloadKind != MediaPayloadKind::Packet &&
                     edge.payloadKind != MediaPayloadKind::Frame)) {
                    continue;
                }
                foundProducerEdge = true;
                if (std::any_of(
                        plan.producers.begin(), plan.producers.end(),
                        [&](const auto& strategy) {
                            return sameKey(strategy, edge);
                        })) {
                    continue;
                }
                auto bound = maximumBytes(node.kind, edge, planningLedger);
                if (!bound) {
                    if (!std::any_of(
                            plan.missingProducers.begin(),
                            plan.missingProducers.end(),
                            [&](const auto& requirement) {
                                return sameKey(requirement, edge);
                            })) {
                        plan.missingProducers.push_back(
                            MediaGraphPayloadProducerRequirement{
                                node.id, edge.streamKind, edge.payloadKind,
                                bound.error().message});
                    }
                    continue;
                }
                if (bound.value() == 0 ||
                    bound.value() > availablePayloadBytes) {
                    return Result::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "single producer payload exceeds the global credit pool"));
                }
                const bool deviceBacked = edge.payloadKind ==
                        MediaPayloadKind::Frame &&
                    edge.hardware.isHardwareBacked();
                plan.producers.push_back(MediaGraphPayloadProducerStrategy{
                    node.id, edge.streamKind, edge.payloadKind,
                    deviceBacked
                        ? MediaGraphPayloadAllocationAccounting::
                              ObservedOnlyExternalBytesAndEngineManagedObject
                        : MediaGraphPayloadAllocationAccounting::
                              EngineManagedBytesAndObject,
                    bound.value(),
                    runtimeIntegrated(node.kind),
                    deviceBacked
                        ? "prepared-logical-frame-bound+device-bytes-observed-only"
                        : "prepared-emission-or-frame-footprint-bound"});
                plan.maximumUnitBytes =
                    (std::max)(plan.maximumUnitBytes, bound.value());
            }
            if (!foundProducerEdge) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "final DAG payload producer has no typed packet/frame output"));
            }
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "payload producer registry"));
    }
    if (!plan.missingProducers.empty() || std::any_of(
            plan.producers.begin(), plan.producers.end(),
            [](const auto& strategy) {
                return !strategy.runtimeIntegrated;
            })) {
        plan.integration = MediaGraphPayloadCreditIntegration::Incomplete;
    }
    if (!plan.isStructurallyValid()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "payload producer registry is structurally incomplete"));
    }
    return Result::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
