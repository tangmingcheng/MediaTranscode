#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
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
        MediaVideoPacketCopyBranchOptions copyOptions;
        copyOptions.prefix = options.prefix + ".copy";
        copyOptions.plan = options.plan;
        copyOptions.queues = options.queues;
        copyOptions.formatSourceNode = options.formatSourceNode;
        copyOptions.formatSourcePort = options.formatSourcePort;
        copyOptions.packetSourceNode = options.packetSourceNode;
        copyOptions.packetSourcePort = options.packetSourcePort;
        copyOptions.muxNode = options.muxNode;
        copyOptions.muxCodecPort = options.muxCodecPort;
        copyOptions.muxPacketPort = options.muxPacketPort;
        buildStatus = MediaVideoPacketCopyBranchBuilder::build(graph, copyOptions);
    } else if (options.plan.branchMode == MediaBranchMode::TranscodeFrame) {
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
        buildStatus = MediaVideoTranscodeBranchBuilder::build(graph, transcodeOptions);
    }

    if (!buildStatus) {
        return ::media::Result<bool>::failure(buildStatus.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
