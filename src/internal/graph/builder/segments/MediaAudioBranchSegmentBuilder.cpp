#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> validateEndpoints(const MediaAudioBranchSegmentOptions& options)
{
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires format source endpoint"));
    }
    if (!options.packetSourceNode.isValid() || options.packetSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires packet source endpoint"));
    }
    if (!options.muxNode.isValid() || options.muxCodecPort.empty() || options.muxPacketPort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires mux endpoints"));
    }
    if (options.queues.metadata == 0 || options.queues.packet == 0 ||
        options.queues.frame == 0 || options.queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder queue capacities must be greater than 0"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> buildCopyBranch(MediaGraph& graph,
                                      const MediaAudioBranchSegmentOptions& options)
{
    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix + ".copy";
    branchOptions.streamKind = MediaStreamKind::Audio;
    branchOptions.sourceStreamIndex = options.plan.sourceStreamIndex;
    branchOptions.formatSourceNode = options.formatSourceNode;
    branchOptions.formatSourcePort = options.formatSourcePort;
    branchOptions.packetSourceNode = options.packetSourceNode;
    branchOptions.packetSourcePort = options.packetSourcePort;
    branchOptions.muxNode = options.muxNode;
    branchOptions.muxCodecPort = options.muxCodecPort;
    branchOptions.muxPacketPort = options.muxPacketPort;
    branchOptions.queues = options.queues;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

::media::Result<void> buildEncodeBranch(MediaGraph& graph,
                                        const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioEncodeBranchOptions encodeOptions;
    encodeOptions.prefix = options.prefix + ".encode";
    encodeOptions.plan = options.plan;
    encodeOptions.parameters = options.parameters;
    encodeOptions.queues = options.queues;
    encodeOptions.formatSourceNode = options.formatSourceNode;
    encodeOptions.formatSourcePort = options.formatSourcePort;
    encodeOptions.packetSourceNode = options.packetSourceNode;
    encodeOptions.packetSourcePort = options.packetSourcePort;
    encodeOptions.muxNode = options.muxNode;
    encodeOptions.muxCodecPort = options.muxCodecPort;
    encodeOptions.muxPacketPort = options.muxPacketPort;
    return MediaAudioEncodeBranchBuilder::build(graph, encodeOptions);
}

} // namespace

::media::Result<bool> MediaAudioBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaAudioBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<bool>::success(false);
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioBranchSegmentBuilder requires planned audio source stream index"));
    }
    if (auto status = validateEndpoints(options); !status) {
        return ::media::Result<bool>::failure(status.error());
    }

    ::media::Result<void> buildStatus = ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("MediaAudioBranchSegmentBuilder unsupported audio branch mode"));
    if (options.plan.branchMode == MediaBranchMode::CopyPacket) {
        buildStatus = buildCopyBranch(graph, options);
    } else if (options.plan.branchMode == MediaBranchMode::TranscodeFrame) {
        buildStatus = buildEncodeBranch(graph, options);
    }

    if (!buildStatus) {
        return ::media::Result<bool>::failure(buildStatus.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
