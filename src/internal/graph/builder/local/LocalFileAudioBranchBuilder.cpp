#include "internal/graph/builder/local/LocalFileAudioBranchBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

#include <cstddef>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "LocalFileAudioBranchBuilder";

::media::Result<void> setNodeOptionChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setAudioSourceStreamOption(MediaGraph& graph, MediaNodeId nodeId, int sourceStreamIndex)
{
    return setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioSourceStreamIndex, std::to_string(sourceStreamIndex));
}

::media::Result<void> applyAudioEncodeOptions(MediaGraph& graph,
                                              MediaNodeId nodeId,
                                              const MediaAudioTranscodeParameters& audio,
                                              const MediaAudioPipelinePlan& plan)
{
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioCodec, plan.targetCodecName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::PlannedEncoder, plan.targetEncoderName); !status) return status;
    if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioRateControl, mediaRateControlModeName(audio.rateControl)); !status) return status;
    if (audio.bitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioBitrateKbps, std::to_string(*audio.bitrateKbps)); !status) return status;
    if (audio.minBitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioMinBitrateKbps, std::to_string(*audio.minBitrateKbps)); !status) return status;
    if (audio.maxBitrateKbps) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioMaxBitrateKbps, std::to_string(*audio.maxBitrateKbps)); !status) return status;
    if (audio.bufferSizeKbits) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioBufferSizeKbits, std::to_string(*audio.bufferSizeKbits)); !status) return status;
    if (audio.sampleRate) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioSampleRate, std::to_string(*audio.sampleRate)); !status) return status;
    if (audio.channels) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioChannels, std::to_string(*audio.channels)); !status) return status;
    if (audio.quality) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioQuality, std::to_string(*audio.quality)); !status) return status;
    if (!audio.preset.empty()) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioPreset, audio.preset); !status) return status;
    if (!audio.profile.empty()) if (auto status = setNodeOptionChecked(graph, nodeId, MediaTranscodeOptionKey::AudioProfile, audio.profile); !status) return status;
    return ::media::Result<void>::success();
}

::media::Result<void> addInputPortChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& name, MediaStreamKind streamKind, MediaEdgeKind edgeKind, MediaPayloadKind payloadKind, bool required, bool multiple)
{
    return MediaGraphBuildSupport::addInputPortChecked(graph, owner, nodeId, name, streamKind, edgeKind, payloadKind, required, multiple);
}

::media::Result<void> addOutputPortChecked(MediaGraph& graph, MediaNodeId nodeId, const std::string& name, MediaStreamKind streamKind, MediaEdgeKind edgeKind, MediaPayloadKind payloadKind, bool required, bool multiple)
{
    return MediaGraphBuildSupport::addOutputPortChecked(graph, owner, nodeId, name, streamKind, edgeKind, payloadKind, required, multiple);
}

::media::Result<void> connectChecked(MediaGraph& graph, MediaNodeId fromNode, const std::string& fromPort, MediaNodeId toNode, const std::string& toPort, const std::string& label, std::size_t capacity)
{
    return MediaGraphBuildSupport::connectChecked(graph,
                                                  owner,
                                                  fromNode,
                                                  fromPort,
                                                  toNode,
                                                  toPort,
                                                  label,
                                                  MediaGraphBuildSupport::blockingQueuePolicy(capacity));
}

::media::Result<void> buildAudioCopyBranch(MediaGraph& graph,
                                           const LocalFileTranscodeOptions& options,
                                           MediaNodeId fileInput,
                                           MediaNodeId split,
                                           MediaNodeId mux,
                                           int streamIndex)
{
    MediaPacketCopyBranchOptions branchOptions;
    branchOptions.prefix = "local.audio.copy";
    branchOptions.streamKind = MediaStreamKind::Audio;
    branchOptions.sourceStreamIndex = streamIndex;
    branchOptions.formatSourceNode = fileInput;
    branchOptions.formatSourcePort = "format";
    branchOptions.packetSourceNode = split;
    branchOptions.packetSourcePort = "audio";
    branchOptions.muxNode = mux;
    branchOptions.queues = options.parameters.queues;
    return MediaPacketCopyBranchBuilder::build(graph, branchOptions);
}

::media::Result<void> buildAudioEncodeBranch(MediaGraph& graph,
                                             const LocalFileTranscodeOptions& options,
                                             MediaNodeId fileInput,
                                             MediaNodeId split,
                                             MediaNodeId mux,
                                             const MediaAudioPipelinePlan& plan)
{
    const MediaGraphQueueParameters& queues = options.parameters.queues;
    const MediaAudioTranscodeParameters& audio = options.parameters.audio;
    const int streamIndex = plan.sourceStreamIndex;
    const MediaNodeId packetNormalize = graph.addNode(MediaNodeKind::PacketNormalize, "local.audio.packet_normalize", "Local audio packet normalize");
    const MediaNodeId codecResolver = graph.addNode(MediaNodeKind::AudioCodecResolver, "local.audio.codec_resolver", "Local audio codec resolver");
    const MediaNodeId decode = graph.addNode(MediaNodeKind::AudioDecode, "local.audio.decode", "Local audio decode");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "local.audio.resample", "Local audio resample");
    const MediaNodeId encode = graph.addNode(MediaNodeKind::AudioEncode, "local.audio.encode", "Local audio encode");

    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph, owner, packetNormalize, MediaStreamKind::Audio, streamIndex); !status) return status;
    if (auto status = setAudioSourceStreamOption(graph, codecResolver, streamIndex); !status) return status;
    if (auto status = applyAudioEncodeOptions(graph, codecResolver, audio, plan); !status) return status;

    const MediaPortId audioPort = graph.addOutputPort(split, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true);
    if (auto status = MediaGraphBuildSupport::requirePort(audioPort, owner, "split.audio"); !status) return status;
    graph.setPortFormatDescriptor(audioPort, MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, streamIndex));

    if (auto status = addInputPortChecked(graph, packetNormalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, packetNormalize, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, codecResolver, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = addOutputPortChecked(graph, codecResolver, "decoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, codecResolver, "encoder", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, decode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, decode, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, decode, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;

    if (auto status = addInputPortChecked(graph, encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addInputPortChecked(graph, encode, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true); !status) return status;
    if (auto status = addOutputPortChecked(graph, encode, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false); !status) return status;
    if (auto status = addOutputPortChecked(graph, encode, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;

    if (auto status = connectChecked(graph, fileInput, "format", packetNormalize, "format", "local.file.input.format -> local.audio.packet_normalize.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, split, "audio", packetNormalize, "packet", "local.stream.split.audio -> local.audio.packet_normalize.packet", queues.packet); !status) return status;
    if (auto status = connectChecked(graph, fileInput, "format", codecResolver, "format", "local.file.input.format -> local.audio.codec_resolver.format", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "decoder", decode, "codec", "local.audio.codec_resolver.decoder -> local.audio.decode.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, packetNormalize, "packet", decode, "packet", "local.audio.packet_normalize.packet -> local.audio.decode.packet", queues.packet); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "encoder", resample, "codec", "local.audio.codec_resolver.encoder -> local.audio.resample.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, decode, "frame", resample, "frame", "local.audio.decode.frame -> local.audio.resample.frame", queues.frame); !status) return status;
    if (auto status = connectChecked(graph, codecResolver, "encoder", encode, "codec", "local.audio.codec_resolver.encoder -> local.audio.encode.codec", queues.metadata); !status) return status;
    if (auto status = connectChecked(graph, resample, "frame", encode, "frame", "local.audio.resample.frame -> local.audio.encode.frame", queues.frame); !status) return status;
    if (auto status = connectChecked(graph, encode, "codec", mux, "codec", "local.audio.encode.codec -> local.file.mux.codec", queues.metadata); !status) return status;
    return connectChecked(graph, encode, "packet", mux, "packet", "local.audio.encode.packet -> local.file.mux.packet", queues.mux);
}

} // namespace

::media::Result<bool> LocalFileAudioBranchBuilder::buildIfPlanned(MediaGraph& graph,
                                                                  const LocalFileTranscodeOptions& options,
                                                                  MediaNodeId fileInput,
                                                                  MediaNodeId split,
                                                                  MediaNodeId mux)
{
    auto plannerOptions = LocalFilePlannerRequestBuilder::buildAudioPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<bool>::failure(plannerOptions.error());
    }

    auto planResult = MediaAudioPipelinePlanner::planFileAudio(options.inputUrl, std::move(plannerOptions).value());
    if (!planResult) {
        return ::media::Result<bool>::failure(planResult.error());
    }

    MediaAudioPipelinePlan plan = std::move(planResult).value();
    if (!plan.enabled || plan.branchMode == MediaBranchMode::Drop) {
        if (auto status = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectAudio, "0"); !status) {
            return ::media::Result<bool>::failure(status.error());
        }
        return ::media::Result<bool>::success(false);
    }

    if (auto muxStatus = setNodeOptionChecked(graph, mux, MediaTranscodeOptionKey::MuxExpectAudio, "1"); !muxStatus) {
        return ::media::Result<bool>::failure(muxStatus.error());
    }
    auto status = plan.branchMode == MediaBranchMode::TranscodeFrame
        ? buildAudioEncodeBranch(graph, options, fileInput, split, mux, plan)
        : buildAudioCopyBranch(graph, options, fileInput, split, mux, plan.sourceStreamIndex);
    if (!status) {
        return ::media::Result<bool>::failure(status.error());
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
