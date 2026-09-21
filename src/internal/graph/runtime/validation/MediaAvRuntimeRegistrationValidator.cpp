#include "internal/graph/runtime/validation/MediaAvRuntimeRegistrationValidator.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"

#include <algorithm>
#include <unordered_set>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

bool connected(const MediaGraph& graph, MediaNodeId from, const char* output,
               MediaNodeId to, const char* input)
{
    const auto* source = graph.findOutputPort(from, output);
    const auto* target = graph.findInputPort(to, input);
    if (!source || !target) return false;
    for (const auto& edge : graph.edges()) {
        if (edge.from.nodeId == from && edge.from.portId == source->id &&
            edge.to.nodeId == to && edge.to.portId == target->id) return true;
    }
    return false;
}

} // namespace

::media::Status MediaAvRuntimeRegistrationValidator::validate(
    const MediaGraph& graph, const MediaAvDomainValidationView& binding)
{
    const auto* shared = std::get_if<MediaAvSharedSourceOutputDomainBinding>(&binding.role);
    const auto* source = std::get_if<MediaAvSourceDomainBinding>(&binding.role);
    const auto* output = std::get_if<MediaAvOutputDomainBinding>(&binding.role);
    const auto* input = shared ? &shared->registration.input : source ? &source->registration.input : nullptr;
    const auto preparationOwner = shared ? shared->registration.preparationOwner : source ? source->registration.preparationOwner : MediaNodeId{};
    const auto outputScheduler = shared ? shared->registration.outputScheduler : output ? output->registration.outputScheduler : MediaNodeId{};
    const auto publisher = shared ? shared->registration.rtpSdpPublisher : output ? output->registration.rtpSdpPublisher : std::optional<MediaNodeId>{};
    const std::span<const MediaNodeId> sourceMembers = shared ? std::span<const MediaNodeId>(shared->registration.processing.source) : source ? std::span<const MediaNodeId>(source->registration.processingMembers) : std::span<const MediaNodeId>{};
    const std::span<const MediaNodeId> outputMembers = shared ? std::span<const MediaNodeId>(shared->registration.processing.output) : output ? std::span<const MediaNodeId>(output->registration.processingMembers) : std::span<const MediaNodeId>{};
    if ((input && sourceMembers.empty()) || ((shared || output) && outputMembers.empty()))
        return invalid("A/V domain requires non-empty ownership for its declared role");
    std::unordered_set<std::uint64_t> members;
    for (const auto partition : {sourceMembers, outputMembers}) {
        for (const auto id : partition) {
            if (!id.isValid() || !graph.findNode(id) ||
                !members.insert(id.value).second)
                return invalid("A/V processing ownership has invalid, duplicate or overlapping members");
        }
    }
    const auto sourceOwned = [&](MediaNodeId id) {
        const auto source = sourceMembers;
        return std::find(source.begin(), source.end(), id) != source.end();
    };
    if (input && (!sourceOwned(input->epochBinder) ||
        !sourceOwned(input->activationSequencer) ||
        !sourceOwned(input->releaseExtractor) ||
        !sourceOwned(preparationOwner) || sourceOwned(outputScheduler)))
        return invalid("A/V registration roles conflict with processing ownership");
    if (members.size() != binding.members.size())
        return invalid("A/V domain registration must cover its exact processing partition");
    const auto matches = [&](MediaNodeId id, MediaNodeKind kind) {
        const auto* node = graph.findNode(id);
        return members.contains(id.value) && node && node->kind == kind;
    };
    if (input && (!matches(input->epochBinder, MediaNodeKind::PlaybackEpochBinder) ||
        !matches(input->activationSequencer, MediaNodeKind::ActivatedStartupReleaseSequencer) ||
        !matches(input->releaseExtractor, MediaNodeKind::AvBoundReleaseExtractor)))
        return invalid("A/V registration role conflicts with its exact node kind");
    if ((shared || output) && !matches(outputScheduler, MediaNodeKind::AvOutputScheduler))
        return invalid("Output registration requires its exact output scheduler");
    if (output && (!output->aggregatePlan || !matches(output->registration.activationOwner, MediaNodeKind::AvContinuousAggregate)))
        return invalid("Continuous output registration requires its typed aggregate owner and plan");
    const auto* owner = input ? graph.findNode(preparationOwner) : nullptr;
    if (input && (!owner || !members.contains(preparationOwner.value) ||
        (owner->kind != MediaNodeKind::VideoFilter &&
         owner->kind != MediaNodeKind::VideoFrameRate) ||
        owner->options.value("video.startup_preparation.owner") != "1"))
        return invalid("A/V registration requires its explicit video preparation owner");
    std::size_t ownerCount = 0;
    std::size_t demuxCount = 0;
    std::size_t publisherCount = 0;
    for (const auto& node : graph.nodes()) {
        if (!binding.contains(node.id)) continue;
        if (node.options.value("video.startup_preparation.owner") == "1") ++ownerCount;
        if (node.kind == MediaNodeKind::DemuxPacketClockBinder) ++demuxCount;
        if (node.kind == MediaNodeKind::MpegTsRtpSdpPublisher) ++publisherCount;
        // Node roles constrain ownership within each explicitly registered domain.
        bool requiresSource = false;
        switch (node.kind) {
        case MediaNodeKind::AvContinuousAggregate:
        case MediaNodeKind::VideoEncode:
        case MediaNodeKind::AudioEncode:
        case MediaNodeKind::EncodedAudioCanonicalizer:
        case MediaNodeKind::AvOutputScheduler:
        case MediaNodeKind::ScheduledOutputRouter:
        case MediaNodeKind::RtpDatagramMaterializer:
        case MediaNodeKind::RtpSdpPublisher:
        case MediaNodeKind::DatagramTransportPlanSource:
        case MediaNodeKind::ScheduledDatagramSender:
        case MediaNodeKind::ProjectMpegTsPlanSource:
        case MediaNodeKind::ScheduledTsAccessUnitAdapter:
        case MediaNodeKind::MpegTsDatagramMaterializer:
        case MediaNodeKind::MpegTsRtpSdpPublisher:
        case MediaNodeKind::FileMux:
            break;
        case MediaNodeKind::RawRtpInput:
        case MediaNodeKind::RealtimeInput:
        case MediaNodeKind::MpegTsDemux:
        case MediaNodeKind::Demux:
        case MediaNodeKind::StreamSplit:
        case MediaNodeKind::RtpClockGroup:
        case MediaNodeKind::RtpClockSnapshotFanout:
        case MediaNodeKind::RtpPacketClockBinder:
        case MediaNodeKind::RtpSourceClockStateAdapter:
        case MediaNodeKind::DemuxPacketClockBinder:
        case MediaNodeKind::SourceClockStateFanout:
        case MediaNodeKind::LockedPacketGate:
        case MediaNodeKind::CanonicalInput:
        case MediaNodeKind::AvStartupCoordinator:
        case MediaNodeKind::AvStartupClock:
        case MediaNodeKind::PlaybackEpochBinder:
        case MediaNodeKind::ActivatedStartupReleaseSequencer:
        case MediaNodeKind::AvBoundReleaseExtractor:
        case MediaNodeKind::PacketStartGate:
        case MediaNodeKind::VideoDecode:
        case MediaNodeKind::HardwareTransfer:
        case MediaNodeKind::VideoTimestamp:
        case MediaNodeKind::VideoFrameRate:
        case MediaNodeKind::VideoFilter:
        case MediaNodeKind::PacketNormalize:
        case MediaNodeKind::PacketSourceConfig:
        case MediaNodeKind::AudioDecode:
        case MediaNodeKind::AudioStartupTrim:
        case MediaNodeKind::AudioDriftController:
        case MediaNodeKind::AudioResample:
            requiresSource = true;
            break;
        case MediaNodeKind::CodecResolver:
        case MediaNodeKind::AudioCodecResolver:
            requiresSource = !output;
            break;
        default:
            return invalid("A/V registration has no processing ownership contract for this node kind");
        }
        if (sourceOwned(node.id) != requiresSource)
            return invalid("A/V node role conflicts with its processing ownership");
        if (shared && node.kind == MediaNodeKind::VideoEncode) {
            auto filtered = requiredBoolNodeOption(&node.options,
                "MediaAvRuntimeRegistrationValidator", "pipeline.filter_active");
            if (!filtered) return ::media::Status::failure(filtered.error());
            const auto expected = filtered.value()
                ? MediaNodeKind::VideoFilter : MediaNodeKind::VideoFrameRate;
            if (owner->kind != expected)
                return invalid("A/V preparation owner disagrees with the planned video pipeline");
        }
    }
    if (ownerCount != (input ? 1u : 0u))
        return invalid("A/V registration requires one preparation owner");
    if (input && (!connected(graph, input->epochBinder, "transaction",
                   input->activationSequencer, "transaction") ||
        !connected(graph, input->epochBinder, "preparation",
                   input->releaseExtractor, "preparation") ||
        !connected(graph, input->activationSequencer, "bound_release",
                   input->releaseExtractor, "bound_release")))
        return invalid("A/V registration startup roles lack their required connections");
    if (demuxCount != (input && input->demuxClock ? 2u : 0u))
        return invalid("A/V registration does not cover the demux clock binders");
    if (input && input->demuxClock) {
        const auto& demux = *input->demuxClock;
        if (!sourceOwned(demux.videoBinder) || !sourceOwned(demux.audioBinder) ||
            demux.videoBinder == demux.audioBinder ||
            !matches(demux.videoBinder, MediaNodeKind::DemuxPacketClockBinder) ||
            !matches(demux.audioBinder, MediaNodeKind::DemuxPacketClockBinder))
            return invalid("A/V demux registration requires distinct typed binders");
        auto video = MediaDemuxPacketClockBinderNodePlanCodec::decode(*graph.findNode(demux.videoBinder));
        auto audio = MediaDemuxPacketClockBinderNodePlanCodec::decode(*graph.findNode(demux.audioBinder));
        if (!video) return ::media::Status::failure(video.error());
        if (!audio) return ::media::Status::failure(audio.error());
        if (video.value().stream != MediaScheduledStream::Video ||
            audio.value().stream != MediaScheduledStream::Audio ||
            video.value().groupKey != binding.groupKey ||
            audio.value().groupKey != binding.groupKey ||
            video.value().mapper != audio.value().mapper)
            return invalid("A/V demux registration conflicts with its stream or clock product");
    }
    if (publisherCount != (publisher ? 1u : 0u) ||
        (publisher &&
         (sourceOwned(*publisher) ||
          !matches(*publisher, MediaNodeKind::MpegTsRtpSdpPublisher))))
        return invalid("A/V registration does not cover its exact SDP publisher");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
