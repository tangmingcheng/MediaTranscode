#include "internal/graph/builder/codec/MediaDecoderOpaqueRetentionOptionCodec.h"
#include "internal/graph/planner/realtime/MediaCompositionSourceRegistryPlanner.h"
#include "internal/graph/builder/codec/MediaDecoderInputRetentionOptionCodec.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceResourcesPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include "internal/graph/planner/realtime/MediaGraphPayloadProducerContract.h"
#include "internal/graph/planner/realtime/MediaGraphFrameCreditContractPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/audio/MediaAudioFrameFootprintPlanner.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include "internal/graph/nodes/input/MediaRtpIngressNodePlanDecoder.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include "internal/graph/nodes/video/MediaVideoFrameReadinessPlanCodec.h"

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
    const auto& runtime = source.runtime;
    const bool hasAudio = runtime.synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && runtime.synchronization.members != MediaTranscodeStreamSet::VideoOnly) ||
        runtime.assembly.audio.has_value() != hasAudio || runtime.audioPipeline.enabled != hasAudio ||
        runtime.audioPipeline.resolvedOutput.has_value() != hasAudio || runtime.audioCorrection.has_value() != hasAudio ||
        runtime.audioPipeline.selectedDecoder.has_value() != hasAudio ||
        runtime.audioPipeline.selectedResampler.has_value() != hasAudio)
        return invalid("Composition resource products differ from source members");
    std::unordered_set<std::uint32_t> members;
    for (const auto id : targets.sourceMembers)
        if (!graph.findNode(id) || !members.insert(id.value).second)
            return invalid("Composition source contains a missing or duplicate node");
    const auto* primary = graph.findNode(targets.primaryInput);
    if (!primary || primary->kind != MediaNodeKind::RawRtpInput)
        return Result::failure(::media::ErrorInfo::unsupported(
            "Composition source resources lack a prepared demux/PES envelope"));
    if (!members.contains(targets.primaryInput.value) || targets.isolatedAudioInput.has_value() != hasAudio ||
        runtime.isolatedAudioInput.has_value() != hasAudio ||
        (hasAudio && (!members.contains(targets.isolatedAudioInput->value) ||
            *targets.isolatedAudioInput == targets.primaryInput)))
        return invalid("Composition raw RTP source requires distinct owned video and audio inputs");
    // Bootstrap raw storage and the continuous protocol/AU budget are separate
    // allocations. Each is shared by exactly this source's AV members.
    if (source.preparationStorage.size() != 2)
        return invalid("Composition source requires raw and continuous preparation allocation owners");
    for (std::size_t allocation = 0; allocation < source.preparationStorage.size(); ++allocation) {
        const auto& reservation = source.preparationStorage[allocation];
        if (!reservation.allocationOwner || !reservation.maximumAllocationBytes ||
            reservation.authority.empty() || reservation.streamCount != (hasAudio ? 2u : 1u))
            return invalid("Composition preparation allocation differs from its source members");
        for (std::size_t prior = 0; prior < allocation; ++prior) {
            const auto& previous = source.preparationStorage[prior].allocationOwner;
            if (previous.get() == reservation.allocationOwner.get() ||
                (!previous.owner_before(reservation.allocationOwner) &&
                 !reservation.allocationOwner.owner_before(previous)))
                return invalid("Composition preparation allocation identity is repeated");
        }
    }
    MediaRealtimeCompositionSourceResources result{targets.sourceIndex, {}, {}};
    result.preparationStorage = source.preparationStorage;
    result.preparationOwners.push_back(targets.primaryInput);
    if (hasAudio) result.preparationOwners.push_back(*targets.isolatedAudioInput);
    auto videoInput = inputFacts(graph, targets.primaryInput, source.input, MediaStreamKind::Video);
    if (!videoInput) return Result::failure(videoInput.error());
    result.inputs.push_back(std::move(videoInput).value());
    if (hasAudio) {
        auto audioInput = inputFacts(graph, *targets.isolatedAudioInput,
            *source.runtime.isolatedAudioInput, MediaStreamKind::Audio);
        if (!audioInput) return Result::failure(audioInput.error());
        result.inputs.push_back(std::move(audioInput).value());
    }
    const auto& startup = runtime.synchronization.startup;
    if (!startup.videoCapacity || !*startup.videoCapacity || startup.audioCapacity.has_value() != hasAudio ||
        (hasAudio && !*startup.audioCapacity))
        return invalid("Composition residence requires sealed prepared startup retention");
    if (!source.video.decoder.preparedInputRetention ||
        (hasAudio && !runtime.audioPipeline.selectedDecoder->preparedInputRetention))
        return invalid("Composition source requires opened video and audio decoder input retention");
    const auto& holders = source.producerHolders;
    if (holders.audio.has_value() != hasAudio ||
        holders.video.decoder != *source.video.decoder.preparedInputRetention ||
        (hasAudio && holders.audio->decoder != *runtime.audioPipeline.selectedDecoder->preparedInputRetention))
        return invalid("Composition producer holders differ from the opened decoder products");
    for (const auto& input : result.inputs) {
        const auto& expected = input.nodeId == targets.primaryInput ? holders.video : *holders.audio;
        if (expected.maximumAtomicInputCompletion != input.payload.maximumPayloadsPerInputCompletion)
            return invalid("Composition producer holders differ from the actual input completion contract");
    }
    std::size_t filters = 0;
    std::size_t videoDecoders = 0;
    std::size_t audioDecoders = 0;
    for (const auto id : targets.sourceMembers) {
        const auto& node = *graph.findNode(id);
        if (node.kind == MediaNodeKind::VideoFilter) {
            ++filters;
            auto execution = MediaVideoFilterExecutionPlanCodec::decode(node.options);
            if (!execution) return Result::failure(execution.error());
            if (!holders.filterRetainedInputFrames ||
                execution.value().output.maximumRetainedInputFrames != *holders.filterRetainedInputFrames ||
                execution.value().output.filterDescription != source.video.filter.filterName)
                return invalid("Composition producer holders differ from the actual filter contract");
        } else if (node.kind == MediaNodeKind::VideoDecode) {
            ++videoDecoders;
            if (std::holds_alternative<MediaDecodedVideoAssemblyTiming>(runtime.assembly.video.timing)) {
                if (!source.video.decoder.preparedOpaqueRetention)
                    return invalid("Decoded source lacks its prepared opaque retention contract");
                auto opaque = MediaDecoderOpaqueRetentionOptionCodec::encode(*source.video.decoder.preparedOpaqueRetention);
                if (!opaque) return Result::failure(opaque.error());
                for (const auto& [key, value] : opaque.value().values())
                    if (!node.options.has(key) || node.options.value(key) != value)
                        return invalid("Composition decoder opaque options differ from prepared facts");
            }
            auto expected = MediaDecoderInputRetentionOptionCodec::encode(holders.video.decoder);
            if (!expected) return Result::failure(expected.error());
            for (const auto& [key, value] : expected.value().values())
                if (node.options.value(key) != value)
                    return invalid("Composition producer holders differ from the actual video decoder contract");
        } else if (node.kind == MediaNodeKind::AudioDecode) {
            ++audioDecoders;
            if (!holders.audio) return invalid("Video-only source has an audio decoder");
            auto expected = MediaDecoderInputRetentionOptionCodec::encode(holders.audio->decoder);
            if (!expected) return Result::failure(expected.error());
            std::size_t codecInputs = 0;
            for (const auto& edge : graph.edges()) {
                if (edge.to.nodeId != id || edge.payloadKind != MediaPayloadKind::CodecContext) continue;
                const auto* resolver = graph.findNode(edge.from.nodeId);
                if (!resolver || resolver->kind != MediaNodeKind::AudioCodecResolver ||
                    !members.contains(resolver->id.value) || edge.streamKind != MediaStreamKind::Audio ||
                    resolver->options.value(MediaTranscodeOptionKey::PlannedDecoder) != runtime.audioPipeline.selectedDecoder->name)
                    return invalid("Composition audio decoder does not use its source codec owner");
                ++codecInputs;
                for (const auto& [key, value] : expected.value().values())
                    if (resolver->options.value(key) != value)
                        return invalid("Composition producer holders differ from the actual audio decoder contract");
            }
            if (codecInputs != 1) return invalid("Composition audio decoder requires one actual codec owner");
        }
    }
    if (videoDecoders != 1 || audioDecoders != (hasAudio ? 1u : 0u) ||
        filters != (holders.filterRetainedInputFrames ? 1u : 0u) ||
        holders.filterRetainedInputFrames.has_value() != source.video.filterActive)
        return invalid("Composition producer holders differ from actual source node membership");
    if (std::holds_alternative<MediaDecodedVideoAssemblyTiming>(runtime.assembly.video.timing)) {
        if (source.video.transferDirection != MediaHardwareTransferDirection::None)
            return invalid("Source lineage allocation requires a planned passthrough transfer");
        std::size_t transfers = 0;
        for (const auto id : targets.sourceMembers) {
            const auto* node = graph.findNode(id);
            if (node->kind != MediaNodeKind::HardwareTransfer) continue;
            if (node->options.value("transfer.direction") != "none")
                return invalid("Source lineage transfer differs from the actual node contract");
            ++transfers;
        }
        if (transfers != 1)
            return invalid("Source lineage requires one actual passthrough transfer node");
        result.sourceTransferDirection = source.video.transferDirection;
        if (!source.preparedPacketContainers || !source.preparedPacketContainers->video.prefixObjects ||
            source.preparedPacketContainers->audio.has_value() != hasAudio ||
            !source.video.decoderOwnershipStorage ||
            source.video.filterActive != source.video.filterOwnershipStorage.has_value())
            return invalid("Prepared source ownership requires its producer allocation geometry");
        const auto decoder = std::find_if(targets.sourceMembers.begin(), targets.sourceMembers.end(),
            [&](MediaNodeId id) { return graph.findNode(id)->kind == MediaNodeKind::VideoDecode; });
        std::optional<MediaNodeId> filter;
        for (const auto id : targets.sourceMembers)
            if (graph.findNode(id)->kind == MediaNodeKind::VideoFilter) filter = id;
        if (!source.video.decoderReadiness || filter.has_value() != source.video.filterReadiness.has_value())
            return invalid("Prepared frame wrapper producer readiness is missing");
        const auto checkReadiness = [&](MediaNodeId id, const MediaVideoFrameProducerFacts& expected) -> ::media::Status {
            auto actual = MediaVideoFrameReadinessPlanCodec::decode(graph.findNode(id)->options);
            if (!actual) return ::media::Status::failure(actual.error());
            if (!actual.value() || *actual.value() != expected)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Frame wrapper readiness differs from actual producer options"));
            return ::media::Status::success();
        };
        if (auto status = checkReadiness(*decoder, *source.video.decoderReadiness); !status) return Result::failure(status.error());
        if (filter) if (auto status = checkReadiness(*filter, *source.video.filterReadiness); !status) return Result::failure(status.error());
        auto expectedRegistry = MediaCompositionSourceRegistryPlanner::plan(source.video);
        if (!expectedRegistry) return Result::failure(expectedRegistry.error());
        if (source.lineageRegistryCapacity != expectedRegistry.value() ||
            graph.findNode(*decoder)->options.value("video.lineage.capacity") != std::to_string(source.lineageRegistryCapacity) ||
            (filter && graph.findNode(*filter)->options.value("video.lineage.capacity") != std::to_string(source.lineageRegistryCapacity)))
            return invalid("Source lineage registry capacity differs from common runtime facts");
        result.sourceLineageRegistryCapacity = source.lineageRegistryCapacity;
        result.preparedPacketContainers = *source.preparedPacketContainers;
        result.packetStorageHolders = holders;
        // The second source reservation is the continuous preparation domain;
        // the first is the separately retained bootstrap raw capture.
        result.frameOwnership.emplace(MediaCompositionFrameOwnershipFacts{
            *decoder, filter, *source.video.decoderOwnershipStorage,
            source.video.filterOwnershipStorage, source.preparationStorage[1].allocationOwner,
            *source.video.decoderReadiness, source.video.filterReadiness});
    }
    if(!runtime.synchronization.members || !startup.videoReadiness)
        return invalid("Composition reference facts lack startup member/readiness products");
    auto routeShape=MediaStartupRoutePlanner::describe(*runtime.synchronization.members,*startup.videoReadiness);
    if(!routeShape)return Result::failure(routeShape.error());
    auto route=MediaStartupRoutePlanner::bind(routeShape.value(),runtime.edgePolicies.metadata,runtime.edgePolicies.atomicMetadata);
    if(!route)return Result::failure(route.error());
    result.referenceFacts.emplace(MediaCompositionSourceResidenceFacts{
        std::move(route).value(),*startup.videoCapacity,
        hasAudio?std::optional<std::uint64_t>{*startup.audioCapacity}:std::nullopt,
        runtime.assembly.video.acquiringCapacity,
        hasAudio?std::optional<std::uint64_t>{runtime.assembly.audio->acquiringCapacity}:std::nullopt,
        static_cast<std::uint64_t>(result.inputs[0].ingress.descriptorCapacity()),
        hasAudio?std::optional<std::uint64_t>{result.inputs[1].ingress.descriptorCapacity()}:std::nullopt,
        holders.filterRetainedInputFrames});
    std::optional<MediaAudioSourceFrameFootprints> audio;
    if (hasAudio) {
        auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(runtime.queues, runtime.audioPipeline);
        if (!bounds) return Result::failure(bounds.error());
        auto correction = MediaAudioCorrectionReachabilityPlanner::plan(runtime.audioServoPolicy,
            MediaAudioSourceCorrectionFacts{runtime.audioPipeline.resolvedOutput->sampleRate(), bounds.value()});
        if (!correction) return Result::failure(correction.error());
        if (!runtime.audioCorrection || *runtime.audioCorrection != correction.value().correction ||
            runtime.synchronization.audioServo.commandLeadNs != correction.value().commandLead ||
            runtime.synchronization.audioServo.compensationWindowNs != correction.value().compensationWindow ||
            runtime.synchronization.audioServo.frequencyFilterTimeConstantNs != correction.value().frequencyFilterTimeConstant)
            return invalid("Composition audio payload facts differ from the actual source correction plan");
        auto footprint = MediaAudioFrameFootprintPlanner::planSource(
            runtime.audioPipeline, correction.value().maximumOutputBlockSamples);
        if (!footprint) return Result::failure(footprint.error());
        audio = std::move(footprint).value();
    }
    for (const auto id : targets.sourceMembers) {
        const auto& node = *graph.findNode(id);
        if (!mediaGraphPayloadProducerOutput(node.kind)) continue;
        bool foundOutput = false;
        for (const auto& edge : graph.edges()) {
            if (edge.from.nodeId != id ||
                (edge.payloadKind != MediaPayloadKind::Packet && edge.payloadKind != MediaPayloadKind::Frame)) continue;
            if (edge.streamKind != MediaStreamKind::Video &&
                (!hasAudio || edge.streamKind != MediaStreamKind::Audio))
                return invalid("Composition producer edge is not a planned source member");
            foundOutput = true;
            if (!acceptsMediaGraphPayloadProducerOutput(node.kind, edge.streamKind, edge.payloadKind))
                return invalid("Composition source producer output differs from its runtime contract");
            if (std::any_of(result.producers.begin(), result.producers.end(), [&](const auto& fact) {
                    return fact.nodeId == id && fact.streamKind == edge.streamKind && fact.payloadKind == edge.payloadKind;
                })) continue;
            std::uint64_t bytes = 0;
            std::string authority;
            if (node.kind == MediaNodeKind::RawRtpInput || node.kind == MediaNodeKind::PacketNormalize ||
                node.kind == MediaNodeKind::RtpPacketClockBinder) {
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
                if (!audio) return invalid("Video-only source contains an audio producer");
                bytes = node.kind == MediaNodeKind::AudioResample ? audio->resampledBytes : audio->decodedBytes;
                authority = "opened-source-decoder+prepared-resampler+correction-window-bound";
            } else {
                return invalid("Composition source contains a producer without source allocation facts");
            }
            std::optional<MediaFrameCreditContract> frame;
            if (edge.payloadKind == MediaPayloadKind::Frame) {
                auto contract = MediaGraphFrameCreditContractPlanner::plan(node, edge.streamKind, bytes);
                if (!contract) return Result::failure(contract.error());
                frame = std::move(contract).value();
            }
            if (bytes == 0 || authority.empty()) return invalid("Composition source has no authoritative payload bound");
            result.producers.push_back({id, edge.streamKind, edge.payloadKind, bytes, std::move(frame), std::move(authority)});
            const bool video = edge.streamKind == MediaStreamKind::Video;
            const MediaCompositionPacketResidenceCapacity capacity{
                video ? (std::holds_alternative<MediaDecodedVideoAssemblyTiming>(runtime.assembly.video.timing)
                    ? 0 : *startup.videoCapacity) : *startup.audioCapacity,
                video ? runtime.assembly.video.acquiringCapacity : runtime.assembly.audio->acquiringCapacity};
            auto residence = MediaCompositionProducerResidencePlanner::plan(source.producerHolders,
                node.kind, edge.streamKind, edge.payloadKind, capacity);
            if (!residence) return Result::failure(residence.error());
            result.residence.push_back({{id, edge.streamKind, edge.payloadKind},
                residence.value().additionalRetainedObjects, residence.value().minimumProgressObjects,
                "opened-decoder-input-retention+source-atomic-input-completion"});
        }
        if (!foundOutput) return invalid("Composition source producer has no packet/frame output");
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRealtimeCompositionSourceResources>::failure(
        ::media::ErrorInfo::allocationFailed("composition source resources"));
}

} // namespace media::ffmpeg::graph
