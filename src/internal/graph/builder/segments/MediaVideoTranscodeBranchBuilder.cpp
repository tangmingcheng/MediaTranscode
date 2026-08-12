#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"

#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaVideoPlanOptionApplier.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeOptionApplier.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoTranscodeBranchBuilder";

MediaVideoTranscodeBranchNodes addVideoTranscodeNodes(MediaGraph& graph,
                                                      const std::string& prefix,
                                                      bool inputStartRequiresKeyFrame,
                                                      bool synchronized,
                                                      bool filterRequired)
{
    MediaVideoTranscodeBranchNodes nodes;
    nodes.codecResolver = graph.addNode(MediaNodeKind::CodecResolver, prefix + ".codec_resolver", "Video codec resolver");
    if (inputStartRequiresKeyFrame) {
        nodes.packetStartGate = graph.addNode(MediaNodeKind::PacketStartGate, prefix + ".packet_start_gate", "Video packet start gate");
    }
    nodes.videoDecode = graph.addNode(MediaNodeKind::VideoDecode, prefix + ".decode", "Video decode");
    nodes.hardwareTransfer = graph.addNode(MediaNodeKind::HardwareTransfer, prefix + ".hwtransfer", "Video hardware frame transfer");
    if (!synchronized) {
        nodes.videoTimestamp = graph.addNode(MediaNodeKind::VideoTimestamp, prefix + ".timestamp", "Video timestamp normalize");
    }
    nodes.videoFrameRate = graph.addNode(MediaNodeKind::VideoFrameRate, prefix + ".framerate", "Video frame rate control");
    if (filterRequired) {
        nodes.videoFilter = graph.addNode(MediaNodeKind::VideoFilter, prefix + ".filter", "Video filter");
    }
    nodes.videoEncode = graph.addNode(MediaNodeKind::VideoEncode, prefix + ".encode", "Video encode");
    return nodes;
}

::media::Result<void> addTranscodePorts(MediaGraph& graph,
                                        const MediaVideoTranscodeBranchOptions& options,
                                        const MediaVideoTranscodeBranchNodes& nodes)
{
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;

    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "source_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (nodes.videoFilter.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    }
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
}

::media::Result<void> connectTranscodePorts(MediaGraph& graph,
                                            const MediaVideoTranscodeBranchOptions& options,
                                            const MediaVideoTranscodeBranchNodes& nodes)
{
    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    const auto& sourcePacketPolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->startupPacket
        : options.canonicalLineageCapacity
            ? policies.atomicVideoPacket
            : policies.videoPacket;
    const auto& videoFramePolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->frame
        : options.canonicalLineageCapacity
            ? policies.synchronizedVideoFrame
            : policies.videoFrame;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", policies.metadata); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.videoDecode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", policies.metadata); !status) return status;
    const MediaNodeId codecTarget = nodes.videoFilter.isValid()
                                        ? nodes.videoFilter
                                        : nodes.videoEncode;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "timestamp_source", nodes.videoTimestamp, "source_codec", options.prefix + ".codec_resolver.timestamp_source -> timestamp.source_codec", policies.metadata); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.videoTimestamp, "target_codec", options.prefix + ".codec_resolver.encoder -> timestamp.target_codec", policies.metadata); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "target_codec", codecTarget, "codec", options.prefix + (nodes.videoFilter.isValid() ? ".timestamp.target_codec -> filter.codec" : ".timestamp.target_codec -> encode.codec"), policies.metadata); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", codecTarget, "codec", options.prefix + (nodes.videoFilter.isValid() ? ".codec_resolver.encoder -> filter.codec" : ".codec_resolver.encoder -> encode.codec"), policies.metadata); !status) return status;
    if (nodes.videoFilter.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", nodes.videoEncode, "codec", options.prefix + ".filter.codec -> encode.codec", policies.metadata); !status) return status;
    }
    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.packetStartGate, "packet", options.prefix + ".packet -> packet_start_gate.packet", sourcePacketPolicy); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.packetStartGate, "packet", nodes.videoDecode, "packet", options.prefix + ".packet_start_gate.packet -> decode.packet", sourcePacketPolicy); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.videoDecode, "packet", options.prefix + ".packet -> decode.packet", sourcePacketPolicy); !status) {
        return status;
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoDecode, "frame", nodes.hardwareTransfer, "frame", options.prefix + ".decode.frame -> hwtransfer.frame", videoFramePolicy); !status) return status;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.hardwareTransfer, "frame", nodes.videoTimestamp, "frame", options.prefix + ".hwtransfer.frame -> timestamp.frame", videoFramePolicy); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "frame", nodes.videoFrameRate, "frame", options.prefix + ".timestamp.frame -> framerate.frame", videoFramePolicy); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.hardwareTransfer, "frame", nodes.videoFrameRate, "frame", options.prefix + ".hwtransfer.frame -> framerate.frame", videoFramePolicy); !status) return status;
    const MediaEdgePolicy& filterOutputPolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->preparedFrame
        : options.canonicalLineageCapacity
            ? policies.preparedVideoFrame
            : policies.videoFrame;
    if (nodes.videoFilter.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFrameRate, "frame", nodes.videoFilter, "frame", options.prefix + ".framerate.frame -> filter.frame", videoFramePolicy); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "frame", nodes.videoEncode, "frame", options.prefix + ".filter.frame -> encode.frame", filterOutputPolicy); !status) return status;
    } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFrameRate, "frame", nodes.videoEncode, "frame", options.prefix + ".framerate.frame -> encode.frame", filterOutputPolicy); !status) {
        return status;
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<MediaEncodedBranchEndpoints> MediaVideoTranscodeBranchBuilder::build(
    MediaGraph& graph,
    const MediaVideoTranscodeBranchOptions& options)
{
    if (options.plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::unsupported("MediaVideoTranscodeBranchBuilder requires transcode_frame video branch"));
    }
    if (options.plan.sourceStreamIndex < 0) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeBranchBuilder requires planned video source stream index"));
    }
    if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(
            graph, owner,
            MediaEndpoint{options.packetSourceNode, options.packetSourcePort},
            MediaStreamKind::Video, MediaEdgeKind::InputPacket,
            options.plan.sourceStreamIndex); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (options.canonicalLineageCapacity) {
        if (!options.generationStartRequiresKeyFrame) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video branch requires planner generation-start key-frame policy"));
        }
        const auto& synchronizedFrames =
            options.edgePolicies.synchronizedVideoFrame.queuePolicy;
        if (!synchronizedFrames.bounded ||
            synchronizedFrames.capacity == 0 ||
            synchronizedFrames.overflowPolicy !=
                MediaQueueOverflowPolicy::BlockProducer ||
            synchronizedFrames.orderingPolicy !=
                MediaQueueOrderingPolicy::Fifo ||
            !synchronizedFrames.preserveOrdering) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video branch requires its planned ordered frame policy"));
        }
        if (!MediaAtomicOutputPolicyContract::accepts(
                options.edgePolicies.preparedVideoFrame)) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video preparation requires a complete planned atomic output policy"));
        }
        if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
    }
    if (options.lineageEdgePolicies) {
        const auto& lineage = *options.lineageEdgePolicies;
        const auto validBlockingPolicy = [](const MediaEdgePolicy& policy) {
            const auto& queue = policy.queuePolicy;
            return queue.bounded && queue.capacity > 0 &&
                queue.overflowPolicy ==
                    MediaQueueOverflowPolicy::BlockProducer &&
                queue.orderingPolicy == MediaQueueOrderingPolicy::Fifo &&
                queue.preserveOrdering;
        };
        if (!MediaAtomicOutputPolicyContract::accepts(
                lineage.startupPacket) ||
            !validBlockingPolicy(lineage.frame) ||
            !MediaAtomicOutputPolicyContract::accepts(
                lineage.preparedFrame)) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video lineage requires complete planned lossless edge policies"));
        }
    }

    MediaVideoTranscodeBranchNodes nodes = addVideoTranscodeNodes(graph,
                                                                  options.prefix,
                                                                  options.inputStartRequiresKeyFrame,
                                                                  options.canonicalLineageCapacity.has_value(),
                                                                  options.plan.filterRequired);
    if (options.inputStartRequiresKeyFrame) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.packetStartGate, "packet_start_gate.require_key_frame", "1"); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (options.canonicalLineageCapacity) {
        if (*options.canonicalLineageCapacity == 0) return ::media::Result<MediaEncodedBranchEndpoints>::failure(::media::ErrorInfo::invalidArgument("Synchronized video branch requires positive lineage capacity"));
        const std::string capacity = std::to_string(*options.canonicalLineageCapacity);
        std::vector<std::pair<MediaNodeId, const char*>> lineageNodes {
            {nodes.videoDecode, "video_decode"},
            {nodes.videoFrameRate, "video_frame_rate"},
            {nodes.videoEncode, "video_encode"},
        };
        if (nodes.videoFilter.isValid()) {
            lineageNodes.emplace_back(nodes.videoFilter, "video_filter");
        }
        for (const auto& [id, identity] : lineageNodes) {
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, "video.lineage.capacity", capacity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, "video.lineage.identity", identity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.videoEncode,
                "video_encode.force_generation_start_key_frame",
                *options.generationStartRequiresKeyFrame ? "1" : "0");
            !status) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                status.error());
        }
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.codecResolver,
                "video.lineage.capacity", capacity); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver, "video.lineage.copy_opaque", "1"); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = MediaVideoTranscodeOptionApplier::applyUserOptions(graph, nodes, options.parameters); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applySelectedPlan(graph, nodes, options.plan); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = connectTranscodePorts(graph, options, nodes); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    return ::media::Result<MediaEncodedBranchEndpoints>::success({
        {nodes.videoEncode, "codec"}, {nodes.videoEncode, "packet"}});
}

} // namespace media::ffmpeg::graph
