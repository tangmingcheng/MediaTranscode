#include "internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

namespace media::ffmpeg::graph {

::media::Result<void> MediaAudioPacketCopyBranchBuilder::build(
    MediaGraph& graph,
    const MediaAudioPacketCopyBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::CopyPacket) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("audio packet copy branch requires CopyPacket mode"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("audio packet copy branch requires a planned source stream index"));
    }
    if (!options.plan.resolvedOutput ||
        options.plan.resolvedOutput->branchMode() != MediaBranchMode::CopyPacket ||
        !options.plan.resolvedOutput->encoderName().empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio packet copy branch requires complete resolved copy output"));
    }
    if (!options.normalizePackets.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("audio packet copy branch requires explicit normalization policy"));
    }

    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix;
    branchOptions.streamKind = MediaStreamKind::Audio;
    branchOptions.sourceStreamIndex = options.plan.sourceStreamIndex;
    branchOptions.formatSourceNode = options.formatSourceNode;
    branchOptions.formatSourcePort = options.formatSourcePort;
    branchOptions.packetSourceNode = options.packetSourceNode;
    branchOptions.packetSourcePort = options.packetSourcePort;
    branchOptions.muxNode = options.muxNode;
    branchOptions.muxCodecPort = options.muxCodecPort;
    branchOptions.muxPacketPort = options.muxPacketPort;
    branchOptions.monotonicPacketTimestamps = options.plan.monotonicPacketTimestamps;
    branchOptions.normalizePackets = options.normalizePackets;
    branchOptions.queues = options.queues;
    branchOptions.edgePolicies = options.edgePolicies;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

} // namespace media::ffmpeg::graph
