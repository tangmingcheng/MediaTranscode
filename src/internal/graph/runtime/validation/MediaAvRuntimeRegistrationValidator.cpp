#include "internal/graph/runtime/validation/MediaAvRuntimeRegistrationValidator.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"

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
    const MediaGraph& graph, const MediaAvSyncRuntimeBinding& binding)
{
    const auto& plan = binding.registration;
    std::unordered_set<std::uint64_t> members;
    for (auto id : plan.members) {
        if (!id.isValid() || !graph.findNode(id) ||
            !members.insert(id.value).second)
            return invalid("A/V registration has invalid or duplicate domain members");
    }
    if (members.size() != graph.nodes().size())
        return invalid("Single A/V domain registration must cover its complete graph");
    const auto matches = [&](MediaNodeId id, MediaNodeKind kind) {
        const auto* node = graph.findNode(id);
        return members.contains(id.value) && node && node->kind == kind;
    };
    if (!matches(plan.input.epochBinder, MediaNodeKind::PlaybackEpochBinder) ||
        !matches(plan.input.activationSequencer, MediaNodeKind::ActivatedStartupReleaseSequencer) ||
        !matches(plan.input.releaseExtractor, MediaNodeKind::AvBoundReleaseExtractor) ||
        !matches(plan.outputScheduler, MediaNodeKind::AvOutputScheduler))
        return invalid("A/V registration role conflicts with its exact node kind");
    const auto* owner = graph.findNode(plan.preparationOwner);
    if (!owner || !members.contains(plan.preparationOwner.value) ||
        (owner->kind != MediaNodeKind::VideoFilter &&
         owner->kind != MediaNodeKind::VideoFrameRate) ||
        owner->options.value("video.startup_preparation.owner") != "1")
        return invalid("A/V registration requires its explicit video preparation owner");
    std::size_t ownerCount = 0;
    std::size_t demuxCount = 0;
    std::size_t publisherCount = 0;
    for (const auto& node : graph.nodes()) {
        if (node.options.value("video.startup_preparation.owner") == "1") ++ownerCount;
        if (node.kind == MediaNodeKind::DemuxPacketClockBinder) ++demuxCount;
        if (node.kind == MediaNodeKind::MpegTsRtpSdpPublisher) ++publisherCount;
        if (node.kind == MediaNodeKind::VideoEncode) {
            auto filtered = requiredBoolNodeOption(&node.options,
                "MediaAvRuntimeRegistrationValidator", "pipeline.filter_active");
            if (!filtered) return ::media::Status::failure(filtered.error());
            const auto expected = filtered.value()
                ? MediaNodeKind::VideoFilter : MediaNodeKind::VideoFrameRate;
            if (owner->kind != expected)
                return invalid("A/V preparation owner disagrees with the planned video pipeline");
        }
    }
    if (ownerCount != 1)
        return invalid("A/V registration requires one preparation owner");
    if (!connected(graph, plan.input.epochBinder, "transaction",
                   plan.input.activationSequencer, "transaction") ||
        !connected(graph, plan.input.epochBinder, "preparation",
                   plan.input.releaseExtractor, "preparation") ||
        !connected(graph, plan.input.activationSequencer, "bound_release",
                   plan.input.releaseExtractor, "bound_release"))
        return invalid("A/V registration startup roles lack their required connections");
    if (demuxCount != (plan.input.demuxClock ? 2u : 0u))
        return invalid("A/V registration does not cover the demux clock binders");
    if (plan.input.demuxClock) {
        const auto& demux = *plan.input.demuxClock;
        if (demux.videoBinder == demux.audioBinder ||
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
    if (publisherCount != (plan.rtpSdpPublisher ? 1u : 0u) ||
        (plan.rtpSdpPublisher &&
         !matches(*plan.rtpSdpPublisher, MediaNodeKind::MpegTsRtpSdpPublisher)))
        return invalid("A/V registration does not cover its exact SDP publisher");
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
