#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> validateEndpoints(const MediaVideoBranchSegmentOptions& options)
{
    if (!options.formatSourceNode.isValid() || options.formatSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires format source endpoint"));
    }
    if (!options.packetSourceNode.isValid() || options.packetSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires packet source endpoint"));
    }
    if (!options.muxNode.isValid() || options.muxCodecPort.empty() || options.muxPacketPort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires mux endpoints"));
    }
    if (options.queues.metadata == 0 || options.queues.packet == 0 ||
        options.queues.frame == 0 || options.queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder queue capacities must be greater than 0"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> buildCopyBranch(MediaGraph& graph,
                                      const MediaVideoBranchSegmentOptions& options)
{
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder video copy requires planned source stream index"));
    }

    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = options.prefix + ".copy";
    branchOptions.streamKind = MediaStreamKind::Video;
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

::media::Result<void> buildTranscodeBranch(MediaGraph& graph,
                                           const MediaVideoBranchSegmentOptions& options)
{
    MediaVideoTranscodeBranchOptions transcodeOptions;
    transcodeOptions.prefix = options.prefix + ".transcode";
    transcodeOptions.plan = options.plan;
    transcodeOptions.parameters = options.parameters;
    transcodeOptions.queues = options.queues;
    transcodeOptions.formatSourceNode = options.formatSourceNode;
    transcodeOptions.formatSourcePort = options.formatSourcePort;
    transcodeOptions.packetSourceNode = options.packetSourceNode;
    transcodeOptions.packetSourcePort = options.packetSourcePort;
    transcodeOptions.muxNode = options.muxNode;
    transcodeOptions.muxCodecPort = options.muxCodecPort;
    transcodeOptions.muxPacketPort = options.muxPacketPort;
    return MediaVideoTranscodeBranchBuilder::build(graph, transcodeOptions);
}

} // namespace

::media::Result<bool> MediaVideoBranchSegmentBuilder::buildIfPlanned(
    MediaGraph& graph,
    const MediaVideoBranchSegmentOptions& options)
{
    if (!options.plan.enabled || options.plan.branchMode == MediaBranchMode::Drop) {
        return ::media::Result<bool>::success(false);
    }

    if (auto status = validateEndpoints(options); !status) {
        return ::media::Result<bool>::failure(status.error());
    }

    ::media::Result<void> buildStatus = ::media::Result<void>::failure(
        ::media::ErrorInfo::unsupported("MediaVideoBranchSegmentBuilder unsupported video branch mode"));
    if (options.plan.branchMode == MediaBranchMode::CopyPacket) {
        buildStatus = buildCopyBranch(graph, options);
    } else if (options.plan.branchMode == MediaBranchMode::TranscodeFrame) {
        buildStatus = buildTranscodeBranch(graph, options);
    }

    if (!buildStatus) {
        return ::media::Result<bool>::failure(buildStatus.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
