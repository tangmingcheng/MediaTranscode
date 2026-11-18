#include "internal/graph/builder/segments/MediaBranchConnectionBindings.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"
#include "internal/graph/planner/video/MediaVideoBranchConnectionsPlanner.h"

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
    if (synchronized && !outputOnly)
        nodes.videoPreparation = graph.addNode(MediaNodeKind::VideoPreparation, prefix + ".preparation", "Video preparation publication");
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
    if (nodes.codecResolver.isValid() && (nodes.hardwareTransfer.isValid() || nodes.videoDecode.isValid())) if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
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
        if (nodes.videoFrameRate.isValid()) {
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        }
        if (nodes.videoFilter.isValid()) {
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, nodes.videoEncode.isValid(), true); !status) return status;
            if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        }
    }
    if (nodes.videoPreparation.isValid()) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoPreparation, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoPreparation, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
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

const MediaEdgePolicy& sourceFramePolicy(const MediaVideoBranchConnectionOptions& options)
{
    return options.lineageEdgePolicies ? options.lineageEdgePolicies->frame :
        options.canonicalLineageCapacity ? options.edgePolicies.synchronizedVideoFrame : options.edgePolicies.videoFrame;
}

::media::Result<MediaVideoBranchBindings> connectTranscodePorts(MediaGraph& graph,
                                            const MediaVideoBranchConnectionOptions& options,
                                            const MediaVideoTranscodeBranchNodes& nodes,
                                            MediaEndpoint outputCodec,
                                            MediaEndpoint sourceFrame,
                                            const MediaEdgePolicy& inputFramePolicy)
{
    const MediaRealtimeEdgePolicySet& policies = options.edgePolicies;
    const auto& sourcePacketPolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->startupPacket
        : options.canonicalLineageCapacity
            ? policies.startupVideoRelease
            : policies.videoPacket;
    const auto& videoFramePolicy = sourceFramePolicy(options);
    using Role = MediaVideoBranchRole;
    std::array<MediaEndpoint, static_cast<std::size_t>(Role::Count)> endpoints{{
        {options.formatSourceNode, options.formatSourcePort},
        {options.packetSourceNode, options.packetSourcePort}, sourceFrame, outputCodec,
        options.sharedDecode ? options.sharedDecode->codec : MediaEndpoint{},
        {nodes.codecResolver, ""}, {nodes.packetStartGate, ""}, {nodes.videoDecode, ""},
        {nodes.sourceCopy, ""}, {nodes.outputFanout, ""}, {nodes.hardwareTransfer, ""},
        {nodes.videoTimestamp, ""}, {nodes.videoFrameRate, ""}, {nodes.videoFilter, ""},
        {nodes.videoPreparation, ""}, {nodes.videoEncode, ""}}};
    MediaVideoBranchShape shape{};
    for (std::size_t i = 0; i < endpoints.size(); ++i) shape.present[i] = endpoints[i].node.isValid();
    const MediaEdgePolicy& filterOutputPolicy = options.lineageEdgePolicies
        ? options.lineageEdgePolicies->preparedFrame
        : options.canonicalLineageCapacity ? policies.preparedVideoFrame : policies.videoFrame;
    auto connections = MediaVideoBranchConnectionsPlanner::plan(shape,
        {policies.metadata, sourcePacketPolicy, videoFramePolicy, inputFramePolicy, filterOutputPolicy}, *std::pmr::new_delete_resource());
    if (!connections) return ::media::Result<MediaVideoBranchBindings>::failure(connections.error());
    MediaVideoBranchBindings bindings{std::move(endpoints)};
    if(auto status=bindings.connect(graph,connections.value(),options.prefix,owner);!status)
        return ::media::Result<MediaVideoBranchBindings>::failure(status.error());
    return ::media::Result<MediaVideoBranchBindings>::success(std::move(bindings));
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
    std::optional<MediaVideoLineagePropagation> encoderLineage,
    MediaVideoDecodeLineageProduct decoderProduct)
{
    if (options.canonicalLineageCapacity) {
        if (*options.canonicalLineageCapacity == 0) return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument("Synchronized video branch requires positive lineage capacity"));
        const std::string capacity = std::to_string(*options.canonicalLineageCapacity);
        const auto productName = mediaVideoDecodeLineageProductName(decoderProduct);
        if (productName.empty() ||
            (decoderProduct == MediaVideoDecodeLineageProduct::SourceAccessUnit &&
             decoderLineage != MediaVideoLineagePropagation::CodecCopyOpaque))
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                "Source-AU decoding requires its planned COPY_OPAQUE lineage"));
        for (const auto id : {nodes.codecResolver, nodes.videoDecode}) {
            if (!id.isValid()) continue;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, id,
                    "video.lineage.decoder_product", std::string(productName)); !status) return status;
        }
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
        if (nodes.videoPreparation.isValid()) {
            const auto readinessOwner = nodes.videoPreparation;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                    graph, owner, readinessOwner,
                    "video.startup_preparation.owner", "1"); !status) {
                return ::media::Result<void>::failure(
                    status.error());
            }
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, readinessOwner,
                    "video.preparation.frame_product", decoderProduct == MediaVideoDecodeLineageProduct::Canonical
                        ? "canonical_interval" : "source_display_point"); !status) return status;
            if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, readinessOwner,
                    "pipeline.filter_active", nodes.videoFilter.isValid() ? "1" : "0"); !status) return status;
        }
        if (nodes.codecResolver.isValid()) if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, nodes.codecResolver,
                "video.lineage.capacity", capacity); !status) return ::media::Result<void>::failure(status.error());
        const char* decoderCopyOpaque =
            decoderLineage ==
                MediaVideoLineagePropagation::CodecCopyOpaque
            ? "1" : "0";
        if (nodes.codecResolver.isValid()) if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodes.codecResolver, "video.lineage.decoder_copy_opaque", decoderCopyOpaque); !status) return ::media::Result<void>::failure(status.error());
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
            options.plan.selected.encoderLineagePropagation, MediaVideoDecodeLineageProduct::Canonical); !status)
        return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = MediaVideoTranscodeOptionApplier::applyUserOptions(graph, nodes, options.parameters); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applySelectedPlan(graph, nodes, options.plan); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes, options.plan.outputFanout.has_value()); !status) return ::media::Result<MediaEncodedBranchEndpoints>::failure(status.error());
    auto bindings = connectTranscodePorts(graph, options, nodes, outputCodec,
        options.sharedDecode ? options.sharedDecode->frame : MediaEndpoint{nodes.videoDecode, "frame"}, sourceFramePolicy(options));
    if (!bindings) return ::media::Result<MediaEncodedBranchEndpoints>::failure(bindings.error());
    MediaProcessingNodeOwnership processing;
    for (const auto id : {nodes.packetStartGate,
             nodes.videoDecode, nodes.sourceCopy, nodes.outputFanout}) {
        if (id.isValid()) processing.source.push_back(id);
    }
    (options.sharedDecode ? processing.output : processing.source).push_back(nodes.codecResolver);
    auto& processingStages = options.sharedDecode || options.plan.outputFanout
        ? processing.output : processing.source;
    for (const auto id : {nodes.hardwareTransfer, nodes.videoTimestamp,
             nodes.videoFrameRate, nodes.videoFilter, nodes.videoPreparation}) {
        if (id.isValid()) processingStages.push_back(id);
    }
    if (nodes.videoEncode.isValid()) processing.output.push_back(nodes.videoEncode);
    return ::media::Result<MediaEncodedBranchEndpoints>::success({
        {nodes.videoEncode, "codec"}, {nodes.videoEncode, "packet"},
        options.canonicalLineageCapacity
            ? std::optional<MediaNodeId>(nodes.videoPreparation)
            : std::nullopt, std::move(processing), std::move(bindings).value()});
}

::media::Result<MediaEncodedBranchEndpoints> MediaVideoTranscodeBranchBuilder::build(
    MediaGraph& graph, const MediaVideoTranscodeBranchOptions& options)
{
    return buildVideoSegment(graph, options);
}

::media::Result<MediaVideoSourceDecoderEndpoints> MediaVideoTranscodeBranchBuilder::buildSourceDecoder(
    MediaGraph& graph, const MediaVideoSourceBranchOptions& options,
    MediaVideoDecodeLineageProduct product)
{
    using Result = ::media::Result<MediaVideoSourceDecoderEndpoints>;
    if (!options.canonicalLineageCapacity || *options.canonicalLineageCapacity == 0 ||
        options.sharedDecode || options.inputStartRequiresKeyFrame ||
        !options.generationStartRequiresKeyFrame || !options.plan.available ||
        options.sourceStreamIndex < 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video source decoder requires explicit source and lineage contracts"));
    if (auto status = validateVideoLineage(options, options.plan.decoderLineagePropagation,
            std::nullopt); !status) return Result::failure(status.error());
    if (auto status = MediaGraphBuildSupport::requirePacketOutputEndpoint(graph, owner,
            {options.packetSourceNode, options.packetSourcePort}, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, options.sourceStreamIndex); !status) return Result::failure(status.error());
    const auto declaration = MediaVideoBranchConnectionsPlanner::sourceDecoder();
    MediaVideoTranscodeBranchNodes nodes;
    const auto add = [&](MediaVideoBranchRole role, const char* suffix, const char* label) {
        return declaration.has(role) ? graph.addNode(*declaration.ownedKind(role), options.prefix + suffix, label)
                                     : MediaNodeId::invalid();
    };
    nodes.codecResolver = add(MediaVideoBranchRole::CodecResolver, ".codec_resolver", "Video codec resolver");
    nodes.videoDecode = add(MediaVideoBranchRole::Decode, ".decode", "Video decode");
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner,
            nodes.codecResolver, "codec_resolver.mode", "source_decode"); !status)
        return Result::failure(status.error());
    if (auto status = configureVideoLineage(graph, options, nodes,
            options.plan.decoderLineagePropagation, std::nullopt, product); !status)
        return Result::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applySourceDecoderPlan(
            graph, nodes, options.plan, options.sourceStreamIndex); !status)
        return Result::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes, false); !status) return Result::failure(status.error());
    auto bindings = connectTranscodePorts(graph, options, nodes, {}, {}, sourceFramePolicy(options));
    if (!bindings) return Result::failure(bindings.error());
    if (auto status = bindings.value().validateDeclaration(graph, declaration); !status)
        return Result::failure(status.error());
    MediaProcessingNodeOwnership processing;
    processing.source = {nodes.codecResolver, nodes.videoDecode};
    return Result::success({{nodes.videoDecode, "frame"}, nodes.codecResolver, std::move(processing), std::move(bindings).value()});
}

::media::Result<MediaVideoSourceFrameEndpoints> MediaVideoTranscodeBranchBuilder::buildSourceFrames(
    MediaGraph& graph, const MediaVideoSourceBranchOptions& options,
    MediaEndpoint sourceFrame, MediaEndpoint outputEncoderCodec,
    const MediaEdgePolicy& inputFramePolicy)
{
    using Result = ::media::Result<MediaVideoSourceFrameEndpoints>;
    if (!sourceFrame.valid() || !outputEncoderCodec.valid() ||
        !options.canonicalLineageCapacity || *options.canonicalLineageCapacity == 0 ||
        options.sharedDecode || options.inputStartRequiresKeyFrame ||
        !options.generationStartRequiresKeyFrame || !options.plan.available)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video source frames require planned lineage and frame/codec endpoints"));
    const auto* framePort = graph.findOutputPort(sourceFrame.node, sourceFrame.port);
    const auto* codecPort = graph.findOutputPort(outputEncoderCodec.node, outputEncoderCodec.port);
    if (!framePort || framePort->streamKind != MediaStreamKind::Video ||
        framePort->edgeKind != MediaEdgeKind::RawFrame || framePort->payloadKind != MediaPayloadKind::Frame ||
        !codecPort || codecPort->streamKind != MediaStreamKind::Video ||
        codecPort->edgeKind != MediaEdgeKind::Metadata || codecPort->payloadKind != MediaPayloadKind::CodecContext)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Video source frame segment requires typed video frame and encoder codec outputs"));
    if (auto status = validateVideoLineage(options, options.plan.decoderLineagePropagation,
            std::nullopt); !status) return Result::failure(status.error());
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&options.timing);
    const auto declaration = MediaVideoBranchConnectionsPlanner::sourceFrames(cadence != nullptr, options.plan.filterActive);
    MediaVideoTranscodeBranchNodes nodes;
    const auto add = [&](MediaVideoBranchRole role, const char* suffix, const char* label) {
        return declaration.has(role) ? graph.addNode(*declaration.ownedKind(role), options.prefix + suffix, label)
                                     : MediaNodeId::invalid();
    };
    nodes.hardwareTransfer = add(MediaVideoBranchRole::Transfer, ".hwtransfer", "Video hardware frame transfer");
    nodes.videoFrameRate = add(MediaVideoBranchRole::FrameRate, ".framerate", "Video frame rate control");
    nodes.videoPreparation = add(MediaVideoBranchRole::Preparation, ".preparation", "Video preparation publication");
    nodes.videoFilter = add(MediaVideoBranchRole::Filter, ".filter", "Video filter");
    if (auto status = configureVideoLineage(graph, options, nodes,
            options.plan.decoderLineagePropagation, std::nullopt,
            cadence ? MediaVideoDecodeLineageProduct::Canonical : MediaVideoDecodeLineageProduct::SourceAccessUnit); !status) return Result::failure(status.error());
    if (auto status = MediaVideoPlanOptionApplier::applySourceFramePlan(graph, nodes, options.plan,
            options.timing, options.maximumFrameDuplicationGap); !status)
        return Result::failure(status.error());
    if (auto status = addTranscodePorts(graph, options, nodes, false); !status) return Result::failure(status.error());
    auto bindings = connectTranscodePorts(graph, options, nodes, outputEncoderCodec, sourceFrame, inputFramePolicy);
    if (!bindings) return Result::failure(bindings.error());
    if (auto status = bindings.value().validateDeclaration(graph, declaration); !status)
        return Result::failure(status.error());
    const auto preparation = nodes.videoPreparation;
    MediaProcessingNodeOwnership processing;
    for (const auto id : {nodes.hardwareTransfer, nodes.videoFrameRate, nodes.videoFilter, nodes.videoPreparation})
        if (id.isValid()) processing.source.push_back(id);
    return Result::success({{preparation, "frame"}, preparation, std::move(processing), std::move(bindings).value()});
}

::media::Result<MediaVideoSourceBranchEndpoints> MediaVideoTranscodeBranchBuilder::buildSource(
    MediaGraph& graph, const MediaVideoSourceBranchOptions& options, MediaEndpoint outputEncoderCodec)
{
    using Result = ::media::Result<MediaVideoSourceBranchEndpoints>;
    if (!std::holds_alternative<MediaObservedVideoCadence>(options.timing))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Presentation source requires its decoded startup segment before frame processing"));
    auto decoder = buildSourceDecoder(graph, options, MediaVideoDecodeLineageProduct::Canonical);
    if (!decoder) return Result::failure(decoder.error());
    auto frames = buildSourceFrames(graph, options, decoder.value().frame, outputEncoderCodec, sourceFramePolicy(options));
    if (!frames) return Result::failure(frames.error());
    auto processing = std::move(decoder.value().processing);
    processing.source.insert(processing.source.end(), frames.value().processing.source.begin(),
        frames.value().processing.source.end());
    return Result::success({frames.value().frame, decoder.value().codecResolver,
        frames.value().preparationOwner, std::move(processing),
        {std::move(decoder.value().bindings), std::move(frames.value().bindings)}});
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
    const auto declaration=MediaVideoBranchConnectionsPlanner::outputEncoder();
    MediaVideoTranscodeBranchNodes nodes;
    nodes.codecResolver=graph.addNode(*declaration.ownedKind(MediaVideoBranchRole::CodecResolver),options.prefix+".codec_resolver","Video codec resolver");
    nodes.videoEncode=graph.addNode(*declaration.ownedKind(MediaVideoBranchRole::Encode),options.prefix+".encode","Video encode");
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
    MediaOutputEncoderEndpoints output;
    output.frameInput = {nodes.videoEncode, "frame"};
    output.codec = codec;
    output.codecResolver = nodes.codecResolver;
    output.encoded.codec = {nodes.videoEncode, "codec"};
    output.encoded.packet = {nodes.videoEncode, "packet"};
    output.encoded.processing.output = {nodes.codecResolver, nodes.videoEncode};
    MediaVideoBranchBindings bindings{};
    bindings.endpoints[static_cast<std::size_t>(MediaVideoBranchRole::CodecResolver)] = {nodes.codecResolver, ""};
    bindings.endpoints[static_cast<std::size_t>(MediaVideoBranchRole::Encode)] = {nodes.videoEncode, ""};
    bindings.endpoints[static_cast<std::size_t>(MediaVideoBranchRole::OutputCodec)] = codec;
    auto connections=MediaVideoBranchConnectionsPlanner::plan(declaration,{options.edgePolicies.metadata,
        options.edgePolicies.videoPacket,options.edgePolicies.videoFrame,options.edgePolicies.videoFrame,options.edgePolicies.preparedVideoFrame}, *std::pmr::new_delete_resource());
    if(!connections)return Result::failure(connections.error());
    if(auto status=bindings.validateDeclaration(graph,declaration);!status)return Result::failure(status.error());
    if(auto status=bindings.connect(graph,connections.value(),options.prefix,owner);!status)return Result::failure(status.error());
    output.encoded.bindings = std::move(bindings);
    return Result::success(std::move(output));
}

} // namespace media::ffmpeg::graph
