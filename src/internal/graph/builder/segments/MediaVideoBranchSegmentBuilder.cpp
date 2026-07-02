#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"

#include <array>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

struct VideoBranchNodeIds {
    MediaNodeId codecResolver = MediaNodeId::invalid();
    MediaNodeId videoDecode = MediaNodeId::invalid();
    MediaNodeId hardwareTransfer = MediaNodeId::invalid();
    MediaNodeId videoTimestamp = MediaNodeId::invalid();
    MediaNodeId videoFrameRate = MediaNodeId::invalid();
    MediaNodeId videoFilter = MediaNodeId::invalid();
    MediaNodeId videoEncode = MediaNodeId::invalid();
};

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                        "MediaVideoBranchSegmentBuilder",
                                                        nodeId,
                                                        key,
                                                        value);
}

::media::Result<void> setIfNotEmpty(MediaGraph& graph,
                                    MediaNodeId nodeId,
                                    const std::string& key,
                                    const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, value);
}

::media::Result<void> setIfPresent(MediaGraph& graph,
                                   MediaNodeId nodeId,
                                   const std::string& key,
                                   const std::optional<int>& value)
{
    if (!value) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, std::to_string(*value));
}

::media::Result<void> validateOptionalNonNegative(const std::optional<int>& value,
                                                  const std::string& name)
{
    if (value && *value < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires non-negative " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateOptionalPositive(const std::optional<int>& value,
                                               const std::string& name)
{
    if (value && *value <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires positive " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateUserVideoOptions(const MediaVideoTranscodeParameters& video)
{
    if (video.width && *video.width <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder rejects zero/negative width; omit width to keep source size"));
    }
    if (video.height && *video.height <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder rejects zero/negative height; omit height to keep source size"));
    }
    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires width and height to be specified together"));
    }
    if (!video.frameRate.complete()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires fps numerator and denominator to be specified together"));
    }

    if (auto status = validateOptionalPositive(video.frameRate.numerator, "fps numerator"); !status) return status;
    if (auto status = validateOptionalPositive(video.frameRate.denominator, "fps denominator"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.bitrateKbps, "video bitrate"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.minBitrateKbps, "video min bitrate"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.maxBitrateKbps, "video max bitrate"); !status) return status;
    if (auto status = validateOptionalPositive(video.bufferSizeKbits, "video buffer size"); !status) return status;
    if (video.minBitrateKbps && video.maxBitrateKbps && *video.minBitrateKbps > *video.maxBitrateKbps) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoBranchSegmentBuilder requires video min bitrate <= max bitrate"));
    }
    if (auto status = validateOptionalNonNegative(video.quality, "quality"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.gop, "gop"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.bFrames, "bframes"); !status) return status;
    return ::media::Result<void>::success();
}

::media::Result<void> applyUserVideoOptionsToNode(MediaGraph& graph,
                                                  MediaNodeId nodeId,
                                                  const MediaVideoTranscodeParameters& video)
{
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoCodec, video.codecName); !status) return status;
    if (auto status = setOption(graph, nodeId, MediaTranscodeOptionKey::VideoRateControl, mediaRateControlModeName(video.rateControl)); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoPreset, video.preset); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoProfile, video.profile); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoTune, video.tune); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoLevel, video.level); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoWidth, video.width); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoHeight, video.height); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsNum, video.frameRate.numerator); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsDen, video.frameRate.denominator); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBitrateKbps, video.bitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoMinBitrateKbps, video.minBitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoMaxBitrateKbps, video.maxBitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBufferSizeKbits, video.bufferSizeKbits); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoQuality, video.quality); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoGop, video.gop); !status) return status;
    return setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBFrames, video.bFrames);
}

::media::Result<void> applyUserVideoOptions(MediaGraph& graph,
                                            const VideoBranchNodeIds& nodes,
                                            const MediaVideoTranscodeParameters& video)
{
    if (auto status = validateUserVideoOptions(video); !status) return status;

    const std::array<MediaNodeId, 6> videoOptionNodes {
        nodes.codecResolver,
        nodes.hardwareTransfer,
        nodes.videoTimestamp,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };

    for (MediaNodeId nodeId : videoOptionNodes) {
        if (auto status = applyUserVideoOptionsToNode(graph, nodeId, video); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> setStageOptions(MediaGraph& graph,
                                      MediaNodeId nodeId,
                                      const std::string& prefix,
                                      const MediaPipelineStagePlan& stage)
{
    if (auto status = setOption(graph, nodeId, prefix + ".component", stage.componentName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".codec", stage.codecName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".ffmpeg", stage.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".filter", stage.filterName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hwaccel", stage.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(stage.frameKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".pixel_format", stage.pixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hw_frames_format", stage.hardwareFramesFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".surface_pixel_format", stage.surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hardware", boolOption(stage.hardware)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy)); !status) return status;
    return setOption(graph, nodeId, prefix + ".score", std::to_string(stage.score));
}

::media::Result<void> setChainOptions(MediaGraph& graph,
                                      MediaNodeId nodeId,
                                      const MediaPipelineChainPlan& chain)
{
    if (auto status = setOption(graph, nodeId, "pipeline.chain", chain.label); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.score", std::to_string(chain.score)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.zero_copy", boolOption(chain.zeroCopy)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.all_hardware", boolOption(chain.allHardware)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.same_hardware_device", boolOption(chain.sameHardwareDevice)); !status) return status;
    return setOption(graph, nodeId, "pipeline.reason", chain.reason);
}

::media::Result<void> setFullPlanOptions(MediaGraph& graph,
                                         MediaNodeId nodeId,
                                         const MediaPipelineChainPlan& chain)
{
    if (auto status = setChainOptions(graph, nodeId, chain); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "filter.pipeline", chain.filter); !status) return status;
    return setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder);
}

::media::Result<void> setCodecResolverEncoderFormatOptions(MediaGraph& graph,
                                                           MediaNodeId codecResolver,
                                                           const MediaPipelineStagePlan& encoder)
{
    if (auto status = setOption(graph, codecResolver, "encoder.pixel_format", encoder.pixelFormat); !status) return status;
    if (auto status = setOption(graph, codecResolver, "encoder.hw_frames_format", encoder.hardwareFramesFormat); !status) return status;
    return setOption(graph, codecResolver, "encoder.surface_pixel_format", encoder.surfacePixelFormat);
}

std::string transferDirectionForPlan(const MediaPipelineChainPlan& chain)
{
    if (chain.decoder.frameKind == MediaHardwareFrameKind::Hardware &&
        chain.filter.frameKind == MediaHardwareFrameKind::Software) {
        return "download";
    }
    if (chain.decoder.frameKind == MediaHardwareFrameKind::Software &&
        chain.filter.frameKind == MediaHardwareFrameKind::Hardware) {
        return "upload";
    }
    return "none";
}

::media::Result<void> applySelectedVideoPlanOptions(MediaGraph& graph,
                                                    const VideoBranchNodeIds& nodes,
                                                    const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& chain = plan.selected;

    const std::array<MediaNodeId, 7> plannedNodes {
        nodes.codecResolver,
        nodes.videoDecode,
        nodes.hardwareTransfer,
        nodes.videoTimestamp,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };
    for (MediaNodeId nodeId : plannedNodes) {
        if (auto status = setFullPlanOptions(graph, nodeId, chain); !status) return status;
    }

    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedDecoder, chain.decoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::VideoCodec, plan.outputCodecName); !status) return status;
    if (auto status = setCodecResolverEncoderFormatOptions(graph, nodes.codecResolver, chain.encoder); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hardware", boolOption(chain.decoder.hardware)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.hwaccel", chain.decoder.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.device", mediaHardwareDeviceKindName(chain.decoder.deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, "pipeline.frame_kind", mediaHardwareFrameKindName(chain.decoder.frameKind)); !status) return status;
    if (auto status = setOption(graph, nodes.hardwareTransfer, "transfer.direction", transferDirectionForPlan(chain)); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, MediaTranscodeOptionKey::PlannedFilter, chain.filter.filterName); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, "filter.name", chain.filter.filterName); !status) return status;
    if (auto status = setOption(graph, nodes.videoFilter, "filter.hwaccel", chain.filter.hwaccelName); !status) return status;
    return setOption(graph, nodes.videoEncode, MediaTranscodeOptionKey::PlannedEncoder, chain.encoder.ffmpegName);
}

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
    constexpr const char* owner = "MediaVideoBranchSegmentBuilder";
    const MediaGraphQueueParameters& queues = options.queues;

    VideoBranchNodeIds nodes;
    nodes.codecResolver = graph.addNode(MediaNodeKind::CodecResolver, options.prefix + ".codec_resolver", "Video codec resolver");
    nodes.videoDecode = graph.addNode(MediaNodeKind::VideoDecode, options.prefix + ".decode", "Video decode");
    nodes.hardwareTransfer = graph.addNode(MediaNodeKind::HardwareTransfer, options.prefix + ".hwtransfer", "Video hardware frame transfer");
    nodes.videoTimestamp = graph.addNode(MediaNodeKind::VideoTimestamp, options.prefix + ".timestamp", "Video timestamp normalize");
    nodes.videoFrameRate = graph.addNode(MediaNodeKind::VideoFrameRate, options.prefix + ".framerate", "Video frame rate control");
    nodes.videoFilter = graph.addNode(MediaNodeKind::VideoFilter, options.prefix + ".filter", "Video filter");
    nodes.videoEncode = graph.addNode(MediaNodeKind::VideoEncode, options.prefix + ".encode", "Video encode");

    if (auto status = applyUserVideoOptions(graph, nodes, options.parameters); !status) return status;
    auto planStatus = MediaPipelinePlanner::applyVideoPlanToGraph(graph,
                                                                  nodes.videoDecode,
                                                                  nodes.videoFilter,
                                                                  nodes.videoEncode,
                                                                  options.plan);
    if (!planStatus) {
        return ::media::Result<void>::failure(planStatus.error());
    }
    if (auto status = applySelectedVideoPlanOptions(graph, nodes, options.plan); !status) return status;

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "decoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "timestamp_source", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.codecResolver, "encoder", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;

    const MediaPortId videoPort = graph.addOutputPort(options.packetSourceNode, options.packetSourcePort, MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(videoPort, owner, options.packetSourcePort); !status) return status;
    if (options.plan.sourceStreamIndex >= 0) {
        graph.setPortFormatDescriptor(videoPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Video, options.plan.sourceStreamIndex));
    }

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.hardwareTransfer, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "source_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "target_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoTimestamp, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFrameRate, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoFilter, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodes.videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodes.videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.formatSourceNode, options.formatSourcePort, nodes.codecResolver, "format", options.prefix + ".format -> codec_resolver.format", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "decoder", nodes.videoDecode, "codec", options.prefix + ".codec_resolver.decoder -> decode.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "timestamp_source", nodes.videoTimestamp, "source_codec", options.prefix + ".codec_resolver.timestamp_source -> timestamp.source_codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.codecResolver, "encoder", nodes.videoTimestamp, "target_codec", options.prefix + ".codec_resolver.encoder -> timestamp.target_codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "target_codec", nodes.videoFilter, "codec", options.prefix + ".timestamp.target_codec -> filter.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", nodes.videoEncode, "codec", options.prefix + ".filter.codec -> encode.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "codec", options.muxNode, options.muxCodecPort, options.prefix + ".filter.codec -> mux.codec", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, options.packetSourceNode, options.packetSourcePort, nodes.videoDecode, "packet", options.prefix + ".packet -> decode.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.packet)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoDecode, "frame", nodes.hardwareTransfer, "frame", options.prefix + ".decode.frame -> hwtransfer.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.hardwareTransfer, "frame", nodes.videoTimestamp, "frame", options.prefix + ".hwtransfer.frame -> timestamp.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoTimestamp, "frame", nodes.videoFrameRate, "frame", options.prefix + ".timestamp.frame -> framerate.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFrameRate, "frame", nodes.videoFilter, "frame", options.prefix + ".framerate.frame -> filter.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoFilter, "frame", nodes.videoEncode, "frame", options.prefix + ".filter.frame -> encode.frame", MediaGraphBuildSupport::blockingQueuePolicy(queues.frame)); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, nodes.videoEncode, "packet", options.muxNode, options.muxPacketPort, options.prefix + ".encode.packet -> mux.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.mux));
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
