#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceResourcesPlanner.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerContract.h"
#include "internal/graph/planner/realtime/MediaGraphFrameCreditContractPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/audio/MediaAudioFrameFootprintPlanner.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"

#include "internal/graph/nodes/input/MediaRtpIngressNodePlanDecoder.h"

#include <algorithm>
#include <new>
#include <unordered_set>

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaCompositionInputAllocationFacts> inputFacts(
    const MediaGraph& graph, MediaNodeId id,
    const MediaRealtimeRtpInputNodePlan& input, MediaStreamKind stream)
{
    using Result = ::media::Result<MediaCompositionInputAllocationFacts>;
    const auto* node = graph.findNode(id);
    if (!node || node->kind != MediaNodeKind::RawRtpInput)
        return Result::failure(::media::ErrorInfo::unsupported(
            "Composition source resources require a prepared raw RTP input envelope; demux/PES facts are not admitted"));
    if (!input.rtpAccessUnitEnvelope || !input.rtpTransport || !input.rtpTransport->ingress)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source lacks its own prepared RTP payload and ingress facts"));
    const auto& payload = *input.rtpAccessUnitEnvelope;
    if (auto status = payload.validate(); !status) return Result::failure(status.error());
    if (auto status = input.rtpTransport->ingress->validateProduct(); !status) return Result::failure(status.error());
    auto ingress = MediaRtpIngressNodePlanDecoder::decode(&node->options);
    if (!ingress) return Result::failure(ingress.error());
    if (!ingress.value().sameProduct(*input.rtpTransport->ingress))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition ingress facts differ from the final input node"));
    if (payload.streamKind != stream ||
        node->options.value("rtp.codec") != payload.codecName ||
        node->options.value("rtp.stream_kind") != (stream == MediaStreamKind::Video ? "video" : "audio") ||
        node->options.value("rtp.access_unit_size_authority") != payload.sizeAuthority ||
        node->options.value("rtp.access_unit_completion_authority") != payload.completionAuthority ||
        node->options.value("rtp.maximum_access_unit_bytes") != std::to_string(payload.maximumAccessUnitBytes) ||
        node->options.value("rtp.maximum_access_units_per_push") != std::to_string(payload.maximumAccessUnitsPerPush))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition input payload differs from its source stream or final node contract"));
    return Result::success({id, payload.asInputPayloadEnvelope(), *input.rtpTransport->ingress});
}

::media::Result<std::uint64_t> videoBytes(const MediaNode& node)
{
    using Result = ::media::Result<std::uint64_t>;
    auto prefix = MediaGraphFrameCreditContractPlanner::videoOutputPrefix(node);
    if (!prefix) return Result::failure(prefix.error());
    auto frame = MediaVideoFrameContractValidator::contractFromOptions(
        &node.options, prefix.value(), "composition source payload");
    if (!frame) return Result::failure(frame.error());
    const auto& descriptor = frame.value();
    return MediaVideoSurfaceFootprint::logicalBytes(descriptor.size.width, descriptor.size.height,
        descriptor.isHardwareBacked() ? descriptor.surfacePixelFormat : descriptor.pixelFormat);
}
} // namespace

::media::Result<MediaRealtimeCompositionSourceResources> MediaRealtimeCompositionSourceResourcesPlanner::plan(
    const MediaGraph& graph, const MediaRealtimeCompositionSourcePlan& source,
    const MediaRealtimeCompositionSourceTargets& targets)
try {
    using Result = ::media::Result<MediaRealtimeCompositionSourceResources>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    std::unordered_set<std::uint32_t> members;
    for (const auto id : targets.sourceMembers)
        if (!graph.findNode(id) || !members.insert(id.value).second)
            return invalid("Composition source contains a missing or duplicate node");
    const auto* primary = graph.findNode(targets.primaryInput);
    if (!primary || primary->kind != MediaNodeKind::RawRtpInput)
        return Result::failure(::media::ErrorInfo::unsupported(
            "Composition source resources lack a prepared demux/PES envelope"));
    if (!members.contains(targets.primaryInput.value) || !targets.isolatedAudioInput ||
        !members.contains(targets.isolatedAudioInput->value) || *targets.isolatedAudioInput == targets.primaryInput ||
        !source.runtime.isolatedAudioInput)
        return invalid("Composition raw RTP source requires distinct owned video and audio inputs");
    MediaRealtimeCompositionSourceResources result{targets.sourceIndex, {}, {}};
    auto videoInput = inputFacts(graph, targets.primaryInput, source.input, MediaStreamKind::Video);
    if (!videoInput) return Result::failure(videoInput.error());
    auto audioInput = inputFacts(graph, *targets.isolatedAudioInput,
        *source.runtime.isolatedAudioInput, MediaStreamKind::Audio);
    if (!audioInput) return Result::failure(audioInput.error());
    result.inputs.push_back(std::move(videoInput).value());
    result.inputs.push_back(std::move(audioInput).value());
    const auto& runtime = source.runtime;
    auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(runtime.queues, runtime.audioPipeline);
    if (!bounds) return Result::failure(bounds.error());
    auto correction = MediaAudioCorrectionReachabilityPlanner::plan(runtime.synchronization,
        MediaAudioSourceCorrectionFacts{runtime.audioPipeline.resolvedOutput->sampleRate(), bounds.value()});
    if (!correction) return Result::failure(correction.error());
    if (!runtime.audioCorrection || *runtime.audioCorrection != correction.value().correction ||
        runtime.synchronization.audioServo.commandLeadNs != correction.value().commandLead ||
        runtime.synchronization.audioServo.compensationWindowNs != correction.value().compensationWindow ||
        runtime.synchronization.audioServo.frequencyFilterTimeConstantNs != correction.value().frequencyFilterTimeConstant)
        return invalid("Composition audio payload facts differ from the actual source correction plan");
    auto audio = MediaAudioFrameFootprintPlanner::planSource(
        runtime.audioPipeline, correction.value().maximumOutputBlockSamples);
    if (!audio) return Result::failure(audio.error());
    for (const auto id : targets.sourceMembers) {
        const auto& node = *graph.findNode(id);
        if (!mediaGraphPayloadProducerOutput(node.kind)) continue;
        bool foundOutput = false;
        for (const auto& edge : graph.edges()) {
            if (edge.from.nodeId != id ||
                (edge.payloadKind != MediaPayloadKind::Packet && edge.payloadKind != MediaPayloadKind::Frame)) continue;
            foundOutput = true;
            if (!acceptsMediaGraphPayloadProducerOutput(node.kind, edge.streamKind, edge.payloadKind))
                return invalid("Composition source producer output differs from its runtime contract");
            if (std::any_of(result.producers.begin(), result.producers.end(), [&](const auto& fact) {
                    return fact.nodeId == id && fact.streamKind == edge.streamKind && fact.payloadKind == edge.payloadKind;
                })) continue;
            std::uint64_t bytes = 0;
            std::string authority;
            if (node.kind == MediaNodeKind::RawRtpInput || node.kind == MediaNodeKind::PacketNormalize) {
                const MediaPreparedInputPayloadBound* bound = nullptr;
                for (const auto& input : result.inputs) {
                    if (node.kind == MediaNodeKind::RawRtpInput && input.nodeId != id) continue;
                    if (const auto* candidate = input.payload.find(edge.streamKind)) {
                        if (bound) return invalid("Composition source packet has ambiguous input allocation facts");
                        bound = candidate;
                    }
                }
                if (!bound) return invalid("Composition source packet lacks its own input allocation facts");
                bytes = bound->maximumPayloadBytes;
                authority = bound->authority;
            } else if (node.kind == MediaNodeKind::VideoDecode || node.kind == MediaNodeKind::VideoFilter ||
                       node.kind == MediaNodeKind::HardwareTransfer) {
                auto footprint = videoBytes(node);
                if (!footprint) return Result::failure(footprint.error());
                bytes = footprint.value();
                authority = "source-stage-frame-contract+logical-image-geometry";
            } else if (node.kind == MediaNodeKind::AudioDecode || node.kind == MediaNodeKind::AudioStartupTrim ||
                       node.kind == MediaNodeKind::AudioResample) {
                bytes = node.kind == MediaNodeKind::AudioResample ? audio.value().resampledBytes : audio.value().decodedBytes;
                authority = "opened-source-decoder+prepared-resampler+correction-window-bound";
            } else {
                return invalid("Composition source contains a producer without source allocation facts");
            }
            std::optional<MediaFrameCreditContract> frame;
            if (edge.payloadKind == MediaPayloadKind::Frame) {
                auto contract = MediaGraphFrameCreditContractPlanner::plan(node, bytes);
                if (!contract) return Result::failure(contract.error());
                frame = std::move(contract).value();
            }
            if (bytes == 0 || authority.empty()) return invalid("Composition source has no authoritative payload bound");
            result.producers.push_back({id, edge.streamKind, edge.payloadKind, bytes, std::move(frame), std::move(authority)});
        }
        if (!foundOutput) return invalid("Composition source producer has no packet/frame output");
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRealtimeCompositionSourceResources>::failure(
        ::media::ErrorInfo::allocationFailed("composition source resources"));
}

} // namespace media::ffmpeg::graph
