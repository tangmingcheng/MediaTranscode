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
                                                      bool filterActive,
                                                      bool sharedDecode,
                                                      bool sourceOnly, bool outputOnly)
{
    MediaVideoTranscodeBranchNodes nodes;
    nodes.codecResolver = graph.addNode(MediaNodeKind::CodecResolver, prefix + ".codec_resolver", "Video codec resolver");
    if (inputStartRequiresKeyFrame && !outputOnly) {
        nodes.packetStartGate = graph.addNode(MediaNodeKind::PacketStartGate, prefix + ".packet_start_gate", "Video packet start gate");
    }
    if (!sharedDecode && !outputOnly) nodes.videoDecode = graph.addNode(MediaNodeKind::VideoDecode, prefix + ".decode", "Video decode");
    if (!outputOnly) {
        nodes.hardwareTransfer = graph.addNode(MediaNodeKind::HardwareTransfer, prefix + ".hwtransfer", "Video hardware frame transfer");
        if (!synchronized) {
            nodes.videoTimestamp = graph.addNode(MediaNodeKind::VideoTimestamp, prefix + ".timestamp", "Video timestamp normalize");
        }
        nodes.videoFrameRate = graph.addNode(MediaNodeKind::VideoFrameRate, prefix + ".framerate", "Video frame rate control");
        if (filterActive) {
            nodes.videoFilter = graph.addNode(MediaNodeKind::VideoFilter, prefix + ".filter", "Video filter");
        }
    }
    if (!sourceOnly) nodes.videoEncode = graph.addNode(MediaNodeKind::VideoEncode, prefix + ".encode", "Video encode");
    return nodes;
}

::media::Result<void> addEncoderPorts(MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes, bool outputFanout)
{
    if (!nodes.videoEncode.isValid()) return ::media::Result<void>::success();
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, !outputFanout, !nodes.hardwareTransfer.isValid()); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "codec_parameters", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecParameters, false, true); !status) return status;
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
}

::media::Result<void> addTranscodePorts(MediaGraph& graph,
                                        const MediaVideoBranchConnectionOptions& options,
                                        const MediaVideoTranscodeBranchNodes& nodes, bool outputFanout)
{
    if (nodes.hardwareTransfer.isValid()) if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (nodes.videoDecode.isValid()) {
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    }
    if (nodes.videoTimestamp.isValid() && !options.sharedDecode) {
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, !outputFanout, true); !status) return status;
    }
    if (nodes.videoDecode.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    }

    if (nodes.packetStartGate.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.packetStartGate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    }
    if (nodes.videoDecode.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    }
    if (nodes.outputFanout.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.outputFanout, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, false); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.outputFanout, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, false, true); !status) return status;
    }
    if (nodes.hardwareTransfer.isValid()) {
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
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, nodes.videoEncode.isValid(), true); !status) return status;
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        }
    }
    return addEncoderPorts(graph, nodes, outputFanout);
}

::media::Result<void> connectVideoCodec(MediaGraph& graph, MediaEndpoint outputCodec,
    MediaNodeId target, const std::string& prefix, bool filter, const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(graph, owner, outputCodec.node,
        outputCodec.port, target, "codec", prefix +
            (filter ? ".codec_resolver.encoder -> filter.codec" : ".codec_resolver.encoder -> encode.codec"), policy);
}

::media::Result<void> connectTranscodePorts(MediaGraph& graph,
                                            const MediaVideoBranchConnectionOptions& options,
                                            const MediaVideoTranscodeBranchNodes& nodes,
                                            MediaEndpoint outputCodec)
{
    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    const auto& sourcePacketPolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->startupPacket
        : options.canonicalLineageCapacity
            ? policies.startupVideoRelease
            : policies.videoPacket;
    const auto& videoFramePolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->frame
        : options.canonicalLineageCapacity
            ? policies.synchronizedVideoFrame
            : policies.videoFrame;
    if (nodes.hardwareTransfer.isValid()) if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", policies.metadata); !status) return status;
    if (nodes.videoDecode.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.videoDecode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", policies.metadata); !status) return status;
    }
    const MediaNodeId codecTarget = nodes.videoFilter.isValid()
                                        ? nodes.videoFilter
                                        : nodes.videoEncode;
    if (nodes.videoTimestamp.isValid()) {
        const auto sourceCodec = options.sharedDecode
            ? options.sharedDecode->codec : MediaEndpoint{nodes.codecResolver, "timestamp_source"};
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, sourceCodec.node, sourceCodec.port, nodes.videoTimestamp, "source_codec", options.prefix + ".source_codec -> timestamp.source_codec", policies.metadata); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, outputCodec.node, outputCodec.port, nodes.videoTimestamp, "target_codec", options.prefix + ".codec_resolver.encoder -> timestamp.target_codec", policies.metadata); !status) return status;
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "target_codec", codecTarget, "codec", options.prefix + (nodes.videoFilter.isValid() ? ".timestamp.target_codec -> filter.codec" : ".timestamp.target_codec -> encode.codec"), policies.metadata); !status) return status;
    } else if (codecTarget.isValid()) if (auto status = connectVideoCodec(graph, outputCodec, codecTarget, options.prefix, nodes.videoFilter.isValid(), policies.metadata); !status) return status;
    if (nodes.videoFilter.isValid() && nodes.videoEncode.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", nodes.videoEncode, "codec", options.prefix + ".filter.codec -> encode.codec", policies.metadata); !status) return status;
    }
    if (!nodes.hardwareTransfer.isValid()) return ::media::Result<void>::success();
    if (nodes.videoDecode.isValid()) {
        if (nodes.packetStartGate.isValid()) {
            if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.packetStartGate, "packet", options.prefix + ".packet -> packet_start_gate.packet", sourcePacketPolicy); !status) return status;
            if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.packetStartGate, "packet", nodes.videoDecode, "packet", options.prefix + ".packet_start_gate.packet -> decode.packet", sourcePacketPolicy); !status) return status;
        } else if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.videoDecode, "packet", options.prefix + ".packet -> decode.packet", sourcePacketPolicy); !status) {
            return status;
        }
    }
    auto sourceFrame = options.sharedDecode
        ? options.sharedDecode->frame : MediaEndpoint{nodes.videoDecode, "frame"};
    if (nodes.sourceCopy.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner,
                sourceFrame.node, sourceFrame.port, nodes.sourceCopy, "frame",
                options.prefix + ".decode.frame -> source_copy.frame", videoFramePolicy); !status) return status;
        sourceFrame = {nodes.sourceCopy, "frame"};
    }
    if (nodes.outputFanout.isValid()) {
        if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner,
                sourceFrame.node, sourceFrame.port, nodes.outputFanout, "frame",
                options.prefix + ".decode.frame -> output_fanout.frame", videoFramePolicy); !status) return status;
        sourceFrame = MediaEndpoint{nodes.outputFanout, "frame"};
    }
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, sourceFrame.node, sourceFrame.port, nodes.hardwareTransfer, "frame", options.prefix + ".decode.frame -> hwtransfer.frame", videoFramePolicy); !status) return status;
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
        if (nodes.videoEncode.isValid()) if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "frame", nodes.videoEncode, "frame", options.prefix + ".filter.frame -> encode.frame", filterOutputPolicy); !status) return status;
    } else if (nodes.videoEncode.isValid()) if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFrameRate, "frame", nodes.videoEncode, "frame", options.prefix + ".framerate.frame -> encode.frame", filterOutputPolicy); !status) {
        return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateVideoLineage(
    const MediaVideoBranchConnectionOptions& options, MediaVideoLineagePropagation decoderLineage,
    std::optional<MediaVideoLineagePropagation> encoderLineage)
{
    if (options.canonicalLineageCapacity) {
        if (!options.generationStartRequiresKeyFrame) {
            return ::media::Result<void>::failure(
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
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video branch requires its planned ordered frame policy"));
        }
        if (!MediaAtomicOutputPolicyContract::accepts(
                options.edgePolicies.preparedVideoFrame)) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video preparation requires a complete planned atomic output policy"));
        }
        if (decoderLineage == MediaVideoLineagePropagation::Unknown ||
            (encoderLineage && *encoderLineage == MediaVideoLineagePropagation::Unknown)) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized video branch requires planner lineage propagation contracts"));
        }
        if (decoderLineage == MediaVideoLineagePropagation::CodecCopyOpaque ||
            (encoderLineage && *encoderLineage == MediaVideoLineagePropagation::CodecCopyOpaque)) {
            if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) {
                return ::media::Result<void>::failure(
                    status.error());
            }
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
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video lineage requires complete planned lossless edge policies"));
        }
    }

    return ::media::Result<void>::success();
}

::media::Result<void> configureEncoderLineage(MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes, std::size_t capacity,
    bool requireGenerationKeyFrame, MediaVideoLineagePropagation propagation)
{
    if (!nodes.videoEncode.isValid()) return ::media::Result<void>::success();
    if (capacity == 0 || propagation == MediaVideoLineagePropagation::Unknown)
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Video encoder requires positive lineage capacity and propagation contract"));
    if (propagation == MediaVideoLineagePropagation::CodecCopyOpaque) {
        if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) return status;
    }
    const auto set = [&](MediaNodeId id, const char* key, const std::string& value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, key, value);
    };
    for (const auto id : {nodes.codecResolver, nodes.videoEncode}) {
        if (auto status = set(id, "video.lineage.capacity", std::to_string(capacity)); !status) return status;
        if (auto status = set(id, "video.lineage.encoder_copy_opaque",
                propagation == MediaVideoLineagePropagation::CodecCopyOpaque ? "1" : "0"); !status) return status;
    }
    if (auto status = set(nodes.videoEncode, "video.lineage.identity", "video_encode"); !status) return status;
    return set(nodes.videoEncode, "video_encode.force_generation_start_key_frame",
        requireGenerationKeyFrame ? "1" : "0");
}

::media::Result<void> configureVideoLineage(
    MediaGraph& graph, const MediaVideoBranchConnectionOptions& options,
    const MediaVideoTranscodeBranchNodes& nodes, MediaVideoLineagePropagation decoderLineage,
    std::optional<MediaVideoLineagePropagation> encoderLineage)
{
    if (options.canonicalLineageCapacity) {
        if (*options.canonicalLineageCapacity == 0) return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument("Synchronized video branch requires positive lineage capacity"));
        const std::string capacity = std::to_string(*options.canonicalLineageCapacity);
        std::vector<std::pair<MediaNodeId, const char*>> lineageNodes {
            {nodes.videoDecode, "video_decode"},
            {nodes.videoFrameRate, "video_frame_rate"},
        };
        if (nodes.videoFilter.isValid()) {
            lineageNodes.emplace_back(nodes.videoFilter, "video_filter");
        }
        for (const auto& [id, identity] : lineageNodes) {
            if (!id.isValid()) continue;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, "video.lineage.capacity", capacity); !status) return ::media::Result<void>::failure(status.error());
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, "video.lineage.identity", identity); !status) return ::media::Result<void>::failure(status.error());
        }
        if (nodes.hardwareTransfer.isValid()) {
            const auto readinessOwner = nodes.videoFilter.isValid()
                ? nodes.videoFilter : nodes.videoFrameRate;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                    graph, owner, readinessOwner,
                    "video.startup_preparation.owner", "1"); !status) {
                return ::media::Result<void>::failure(
                    status.error());
            }
        }
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.codecResolver,
                "video.lineage.capacity", capacity); !status) return ::media::Result<void>::failure(status.error());
        const char* decoderCopyOpaque =
            decoderLineage ==
                MediaVideoLineagePropagation::CodecCopyOpaque
            ? "1" : "0";
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver, "video.lineage.decoder_copy_opaque", decoderCopyOpaque); !status) return ::media::Result<void>::failure(status.error());
        if (nodes.videoDecode.isValid()) if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.videoDecode, "video.lineage.decoder_copy_opaque", decoderCopyOpaque); !status) return ::media::Result<void>::failure(status.error());
        if (encoderLineage) {
            if (auto status = configureEncoderLineage(graph, nodes, *options.canonicalLineageCapacity,
                    *options.generationStartRequiresKeyFrame, *encoderLineage); !status) return status;
        }
    }
    return ::media::Result<void>::success();
}

} // namespace

static ::media::Result<MediaEncodedBranchEndpoints> buildVideoSegment(
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
    if (!options.sharedDecode) {
        if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(
                graph, owner,
                MediaEndpoint{options.packetSourceNode, options.packetSourcePort},
                MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                options.plan.sourceStreamIndex); !status) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
    } else if (options.inputStartRequiresKeyFrame || options.canonicalLineageCapacity ||
               !options.sharedDecode->frame.node.isValid() ||
               !options.sharedDecode->codec.node.isValid()) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("shared decode branch requires video-only frame and source codec endpoints"));
    }
    if (auto status = validateVideoLineage(options, options.plan.selected.decoderLineagePropagation,
            options.plan.selected.encoderLineagePropagation); !status)
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());

    MediaVideoTranscodeBranchNodes nodes = addVideoTranscodeNodes(graph,
                                                                  options.prefix,
                                                                  options.inputStartRequiresKeyFrame,
                                                                  options.canonicalLineageCapacity.has_value(),
                                                                  options.plan.filterActive,
                                                                  options.sharedDecode.has_value(), false, false);
    const MediaEndpoint outputCodec{nodes.codecResolver, "encoder"};
    if (options.plan.outputFanout && !options.sharedDecode) {
        if (!options.plan.sharedSource) return ::media::Result<MediaEncodedBranchEndpoints>::failure(
            ::media::ErrorInfo::notInitialized("shared video output requires its source allocation contract"));
        const auto& source = *options.plan.sharedSource;
        if ((source.allocation == MediaVideoSourceAllocation::IndependentFilterOutput) != source.copy.has_value())
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument("shared video source allocation contradicts its copy contract"));
        if (source.copy) {
            nodes.sourceCopy = graph.addNode(MediaNodeKind::VideoFilter,
                options.prefix + ".source_copy", "Shared video source isolation");
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.sourceCopy,
                    "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status)
                return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.sourceCopy,
                    "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status)
                return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
            if (auto status = MediaVideoPlanOptionApplier::applyFilterExecutionPlan(graph, nodes.sourceCopy, *source.copy); !status)
                return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
        if (options.plan.outputFanout->noOutputs != MediaVideoNoOutputPolicy::Consume ||
            options.plan.outputFanout->overflow != MediaVideoOutputOverflowPolicy::FailBranch) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::unsupported("unsupported planned video output fanout policy"));
        }
        if (!options.plan.sourcePlaybackEpoch) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::notInitialized("video distributor lacks a planned playback epoch authority"));
        }
        const auto& epoch = *options.plan.sourcePlaybackEpoch;
        const bool sessionEpoch = epoch.authority == MediaVideoSourceEpochAuthority::ProtocolSession;
        if (sessionEpoch != epoch.protocolSessionEpoch.has_value() ||
            epoch.identityTransition != MediaVideoSourceIdentityTransition::ReplanSession) {
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument("video source playback epoch authority conflicts with its value"));
        }
        nodes.outputFanout = graph.addNode(MediaNodeKind::VideoOutputFanout,
            options.prefix + ".output_fanout", "Video output branch distributor");
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.outputFanout, "fanout.epoch.authority",
                sessionEpoch ? "protocol_session" : "canonical_lineage"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.outputFanout, "fanout.epoch.identity_transition", "replan_session"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        if (sessionEpoch) {
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                    graph, owner, nodes.outputFanout, "fanout.epoch.protocol_session",
                    std::to_string(*epoch.protocolSessionEpoch)); !status)
                return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.outputFanout, "fanout.zero_outputs", "consume"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.outputFanout, "fanout.overflow", "fail_branch"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (options.sharedDecode) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.codecResolver, "codec_resolver.mode", "output_branch"); !status)
            return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (nodes.packetStartGate.isValid()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.packetStartGate, "packet_start_gate.require_key_frame", "1"); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    if (auto status = configureVideoLineage(graph, options, nodes,
            options.plan.selected.decoderLineagePropagation,
            options.plan.selected.encoderLineagePropagation); !status)
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = MediaVideoTranscodeOptionApplier::applyUserOptions(graph, nodes, options.parameters); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applySelectedPlan(graph, nodes, options.plan); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes, options.plan.outputFanout.has_value()); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = connectTranscodePorts(graph, options, nodes, outputCodec); !status) {
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    }
    MediaProcessingNodeOwnership processing;
    for (const auto id : {nodes.packetStartGate,
             nodes.videoDecode, nodes.sourceCopy, nodes.outputFanout}) {
        if (id.isValid()) processing.source.push_back(id);
    }
    (options.sharedDecode ? processing.output : processing.source).push_back(nodes.codecResolver);
    auto& processingStages = options.sharedDecode || options.plan.outputFanout
        ? processing.output : processing.source;
    for (const auto id : {nodes.hardwareTransfer, nodes.videoTimestamp,
             nodes.videoFrameRate, nodes.videoFilter}) {
        if (id.isValid()) processingStages.push_back(id);
    }
    if (nodes.videoEncode.isValid()) processing.output.push_back(nodes.videoEncode);
    return ::media::Result<MediaEncodedBranchEndpoints>::success({
        {nodes.videoEncode, "codec"}, {nodes.videoEncode, "packet"},
        options.canonicalLineageCapacity
            ? std::optional<MediaNodeId>(nodes.videoFilter.isValid()
                  ? nodes.videoFilter : nodes.videoFrameRate)
            : std::nullopt, std::move(processing)});
}

::media::Result<MediaEncodedBranchEndpoints> MediaVideoTranscodeBranchBuilder::build(
    MediaGraph& graph, const MediaVideoTranscodeBranchOptions& options)
{
    return buildVideoSegment(graph, options);
}

::media::Result<MediaSourceBranchEndpoints> MediaVideoTranscodeBranchBuilder::buildSource(
    MediaGraph& graph, const MediaVideoSourceBranchOptions& options, MediaEndpoint outputEncoderCodec)
{
    using Result = ::media::Result<MediaSourceBranchEndpoints>;
    if (!outputEncoderCodec.valid() || !options.canonicalLineageCapacity ||
        *options.canonicalLineageCapacity == 0 || options.sharedDecode ||
        options.inputStartRequiresKeyFrame || !options.generationStartRequiresKeyFrame ||
        !options.plan.available || options.sourceStreamIndex < 0 ||
        options.plan.decoderLineagePropagation == MediaVideoLineagePropagation::Unknown)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video source requires planned decoder, canonical lineage and shared output codec"));
    if (auto status = validateVideoLineage(options, options.plan.decoderLineagePropagation,
            std::nullopt); !status) return Result::failure(status.error());
    if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(graph, owner,
            {options.packetSourceNode, options.packetSourcePort}, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, options.sourceStreamIndex); !status) return Result::failure(status.error());
    const auto nodes = addVideoTranscodeNodes(graph, options.prefix, false, true,
        options.plan.filterActive, false, true, false);
    const auto set = [&](MediaNodeId id, const char* key, const std::string& value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id, key, value);
    };
    if (auto status = set(nodes.codecResolver, "codec_resolver.mode", "source_decode"); !status)
        return Result::failure(status.error());
    if (auto status = configureVideoLineage(graph, options, nodes,
            options.plan.decoderLineagePropagation, std::nullopt); !status) return Result::failure(status.error());
    const auto preparation = nodes.videoFilter.isValid() ? nodes.videoFilter : nodes.videoFrameRate;
    if (auto status = MediaVideoPlanOptionApplier::applySourcePlan(graph, nodes, options.plan,
            options.sourceStreamIndex, options.frameRate, options.maximumFrameDuplicationGap); !status)
        return Result::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes, false); !status) return Result::failure(status.error());
    if (auto status = connectTranscodePorts(graph, options, nodes, outputEncoderCodec); !status) return Result::failure(status.error());
    MediaProcessingNodeOwnership processing;
    for (const auto id : {nodes.codecResolver, nodes.videoDecode, nodes.hardwareTransfer,
             nodes.videoFrameRate, nodes.videoFilter}) {
        if (id.isValid()) processing.source.push_back(id);
    }
    return Result::success({{preparation, "frame"}, nodes.codecResolver, preparation, std::move(processing)});
}

::media::Result<MediaOutputEncoderEndpoints> MediaVideoTranscodeBranchBuilder::buildOutputEncoder(
    MediaGraph& graph, const MediaVideoOutputEncoderOptions& options)
{
    using Result = ::media::Result<MediaOutputEncoderEndpoints>;
    const auto& encoder = options.plan.encoder;
    if (options.prefix.empty() || options.canonicalLineageCapacity == 0 ||
        encoder.role != MediaPipelineStageRole::Encoder || !encoder.available ||
        encoder.codecName.empty() || !encoder.encodedPacketLayout || !encoder.preparedEmission ||
        !encoder.randomAccess || !encoder.encoderOpenContract ||
        options.plan.encoderLineagePropagation == MediaVideoLineagePropagation::Unknown ||
        options.plan.encoderAbortPolicy == MediaVideoEncoderAbortPolicy::Unknown)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output encoder requires its selected encoder, readback and lineage contracts"));
    if (!MediaAtomicOutputPolicyContract::accepts(options.edgePolicies.preparedVideoFrame))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output encoder requires a planned atomic input frame policy"));
    const auto nodes = addVideoTranscodeNodes(graph, options.prefix, false, true, false, false, false, true);
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver,
            "codec_resolver.mode", "output_branch"); !status) return Result::failure(status.error());
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver,
            MediaTranscodeOptionKey::VideoCodec, encoder.codecName); !status) return Result::failure(status.error());
    if (auto status = configureEncoderLineage(graph, nodes, options.canonicalLineageCapacity,
            options.generationStartRequiresKeyFrame, options.plan.encoderLineagePropagation); !status)
        return Result::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applyEncoderPlan(
            graph, nodes, encoder, options.plan.encoderAbortPolicy); !status)
        return Result::failure(status.error());
    if (auto status = addEncoderPorts(graph, nodes, false); !status) return Result::failure(status.error());
    const MediaEndpoint codec{nodes.codecResolver, "encoder"};
    if (auto status = connectVideoCodec(graph, codec, nodes.videoEncode, options.prefix,
            false, options.edgePolicies.metadata); !status) return Result::failure(status.error());
    MediaOutputEncoderEndpoints output;
    output.frameInput = {nodes.videoEncode, "frame"};
    output.codec = codec;
    output.codecResolver = nodes.codecResolver;
    output.encoded.codec = {nodes.videoEncode, "codec"};
    output.encoded.packet = {nodes.videoEncode, "packet"};
    output.encoded.processing.output = {nodes.codecResolver, nodes.videoEncode};
    return Result::success(std::move(output));
}

} // namespace media::ffmpeg::graph
