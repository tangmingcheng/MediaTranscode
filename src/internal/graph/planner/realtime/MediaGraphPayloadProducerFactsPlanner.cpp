#include "internal/graph/planner/realtime/MediaGraphPayloadProducerFactsPlanner.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerContract.h"
#include <algorithm>
#include "internal/graph/planner/realtime/MediaGraphFrameCreditContractPlanner.h"
#include <utility>
#include <new>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::uint64_t> maximumBytes(
    MediaNodeKind producerKind,
    const MediaEdge& edge,
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (producerKind == MediaNodeKind::RawRtpInput ||
        producerKind == MediaNodeKind::Demux ||
        producerKind == MediaNodeKind::MpegTsDemux ||
        producerKind == MediaNodeKind::PacketNormalize) {
        if (!ledger.preparedInputPayload ||
            !ledger.preparedInputPayload->validate()) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry lacks a prepared input allocation envelope"));
        }
        const auto expectedSource = producerKind == MediaNodeKind::RawRtpInput
            ? MediaPreparedInputPayloadSource::RawRtpAccessUnit
            : producerKind == MediaNodeKind::Demux
            ? MediaPreparedInputPayloadSource::GenericDemuxPacket
            : producerKind == MediaNodeKind::MpegTsDemux
                ? MediaPreparedInputPayloadSource::MpegTsPesPacket
                : ledger.preparedInputPayload->source;
        if (ledger.preparedInputPayload->source != expectedSource) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry input source conflicts with the final DAG"));
        }
        const auto* bound = ledger.preparedInputPayload->find(edge.streamKind);
        if (!bound) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry input envelope lacks the selected stream"));
        }
        return ::media::Result<std::uint64_t>::success(
            bound->maximumPayloadBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Video &&
        producerKind == MediaNodeKind::VideoEncode) {
        return ::media::Result<std::uint64_t>::success(
            ledger.media.videoUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Audio &&
        producerKind == MediaNodeKind::AudioEncode &&
        ledger.media.audioUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.media.audioUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Video &&
        (producerKind == MediaNodeKind::VideoDecode ||
         producerKind == MediaNodeKind::HardwareTransfer ||
         producerKind == MediaNodeKind::VideoFilter)) {
        return ::media::Result<std::uint64_t>::success(
            ledger.videoSurfaceUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Audio &&
        (producerKind == MediaNodeKind::AudioDecode ||
         producerKind == MediaNodeKind::AudioStartupTrim ||
         producerKind == MediaNodeKind::AudioResample) &&
        ledger.audioFrameUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.audioFrameUnitBytes);
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "producer registry lacks a prepared payload bound"));
}

std::string allocationAuthority(
    MediaNodeKind producerKind,
    const MediaEdge& edge,
    const MediaRealtimeGraphResourceLedgerPlan& ledger,
    bool deviceBacked)
{
    if ((producerKind == MediaNodeKind::RawRtpInput ||
         producerKind == MediaNodeKind::Demux ||
         producerKind == MediaNodeKind::MpegTsDemux ||
         producerKind == MediaNodeKind::PacketNormalize) &&
        ledger.preparedInputPayload) {
        const auto* bound = ledger.preparedInputPayload->find(edge.streamKind);
        if (bound) return bound->authority;
    }
    return deviceBacked
        ? "prepared-logical-frame-bound+device-bytes-observed-only"
        : "prepared-encoder-emission-or-frame-footprint-bound";
}

} // namespace

::media::Result<std::vector<MediaGraphPayloadProducerFact>> MediaGraphPayloadProducerFactsPlanner::plan(
    const MediaGraph& graph, const MediaRealtimeGraphResourceLedgerPlan& ledger,
    std::span<const MediaNodeId> selectedNodes)
try {
    using Result = ::media::Result<std::vector<MediaGraphPayloadProducerFact>>;
    std::vector<MediaGraphPayloadProducerFact> facts;
    for (const auto& node : graph.nodes()) {
        if (!selectedNodes.empty() && std::find(selectedNodes.begin(), selectedNodes.end(), node.id) == selectedNodes.end()) continue;
        if (!mediaGraphPayloadProducerOutput(node.kind)) continue;
        for (const auto& edge : graph.edges()) {
            if (edge.from.nodeId != node.id ||
                (edge.payloadKind != MediaPayloadKind::Packet && edge.payloadKind != MediaPayloadKind::Frame)) continue;
            if (std::any_of(facts.begin(), facts.end(), [&](const auto& fact) {
                    return fact.nodeId == node.id && fact.streamKind == edge.streamKind && fact.payloadKind == edge.payloadKind;
                })) continue;
            auto bound = maximumBytes(node.kind, edge, ledger);
            if (!bound) return Result::failure(bound.error());
            std::optional<MediaFrameCreditContract> frame;
            if (edge.payloadKind == MediaPayloadKind::Frame) {
                auto contract = MediaGraphFrameCreditContractPlanner::plan(node, bound.value());
                if (!contract) return Result::failure(contract.error());
                frame = std::move(contract).value();
            }
            const bool device = frame && frame->allocationScope == MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly;
            facts.push_back({node.id, edge.streamKind, edge.payloadKind, bound.value(),
                std::move(frame), allocationAuthority(node.kind, edge, ledger, device)});
        }
    }
    return Result::success(std::move(facts));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaGraphPayloadProducerFact>>::failure(
        ::media::ErrorInfo::allocationFailed("payload producer facts"));
}

} // namespace media::ffmpeg::graph
