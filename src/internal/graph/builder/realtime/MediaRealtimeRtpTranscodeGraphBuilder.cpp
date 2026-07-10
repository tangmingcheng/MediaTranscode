#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

struct RealtimePacketInputChain {
    MediaNodeId input;
    PacketSelectSegment packetSelect;
};

::media::Result<void> addRealtimeInputPorts(MediaGraph& graph, MediaNodeId input)
{
    return MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                        owner,
                                                        input,
                                                        "format",
                                                        MediaStreamKind::Metadata,
                                                        MediaEdgeKind::Metadata,
                                                        MediaPayloadKind::FormatContext,
                                                        true,
                                                        true);
}

::media::Result<MediaNodeId> addRawRtpPacketNormalize(MediaGraph& graph,
                                                      MediaNodeId formatSource,
                                                      MediaNodeId packetSource,
                                                      int sourceStreamIndex,
                                                      const MediaRealtimeEdgePolicySet& edgePolicies)
{
    const MediaNodeId normalize = graph.addNode(MediaNodeKind::PacketNormalize,
                                                "realtime.raw_rtp.normalize",
                                                "Raw RTP packet normalize");
    if (!normalize.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError("MediaRealtimeRtpTranscodeGraphBuilder failed to add raw RTP packet normalize node"));
    }
    if (auto status = MediaGraphBuildSupport::setPacketNormalizeOptions(graph,
                                                                        owner,
                                                                        normalize,
                                                                        MediaStreamKind::Video,
                                                                        sourceStreamIndex,
                                                                        false); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, normalize, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, normalize, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    const MediaPortId splitVideo = graph.addOutputPort(packetSource,
                                                       "video",
                                                       MediaStreamKind::Video,
                                                       MediaEdgeKind::InputPacket,
                                                       MediaPayloadKind::Packet,
                                                       false,
                                                       true);
    if (auto status = MediaGraphBuildSupport::requirePort(splitVideo, owner, "raw RTP stream split video"); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    graph.setPortFormatDescriptor(splitVideo,
                                  MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Video,
                                                                                sourceStreamIndex));

    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, formatSource, "format", normalize, "format", "realtime.raw_rtp.format -> normalize.format", edgePolicies.metadata); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, packetSource, "video", normalize, "packet", "realtime.raw_rtp.packet -> normalize.packet", edgePolicies.videoPacket); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    return ::media::Result<MediaNodeId>::success(normalize);
}

::media::Result<RealtimePacketInputChain> addRealtimePacketInputChain(
    MediaGraph& graph,
    MediaNodeKind inputKind,
    const std::string& prefix,
    const std::string& label,
    const MediaRealtimeRtpInputNodePlan& inputPlan,
    const MediaGraphQueueParameters& queues,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    const MediaNodeId input = graph.addNode(inputKind, prefix, label);
    if (!input.isValid()) {
        return ::media::Result<RealtimePacketInputChain>::failure(
            ::media::ErrorInfo::internalError("MediaRealtimeRtpTranscodeGraphBuilder failed to add input node"));
    }
    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, inputPlan); !status) {
        return ::media::Result<RealtimePacketInputChain>::failure(status.error());
    }
    if (auto status = addRealtimeInputPorts(graph, input); !status) {
        return ::media::Result<RealtimePacketInputChain>::failure(status.error());
    }

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = prefix;
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = queues;
    packetSelectOptions.edgePolicies = edgePolicies;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<RealtimePacketInputChain>::failure(packetSelect.error());
    }

    RealtimePacketInputChain chain;
    chain.input = input;
    chain.packetSelect = packetSelect.value();
    return ::media::Result<RealtimePacketInputChain>::success(chain);
}

::media::Result<void> addRtpOutputChain(MediaGraph& graph,
                                        MediaNodeId output,
                                        MediaNodeId mux,
                                        MediaNodeId sdp,
                                        MediaStreamKind streamKind,
                                        const MediaRealtimeEdgePolicySet& edgePolicies)
{
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, output, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "codec", streamKind, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "packet", streamKind, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, output, "format", mux, "format", "realtime.rtp.output.format -> realtime.rtp.mux.format", edgePolicies.metadata); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, mux, "format", sdp, "format", "realtime.rtp.mux.format -> realtime.sdp.writer.format", edgePolicies.metadata, false);
}

bool branchEnabled(const MediaAudioPipelinePlan& plan) noexcept
{
    return plan.enabled && plan.branchMode != MediaBranchMode::Drop;
}

bool separateRtpOutput(const MediaRealtimeRtpTranscodePlan& plan) noexcept
{
    return plan.outputLayout == RealtimeOutputStreamLayout::SeparateStreams;
}

::media::Result<MediaNodeId> addAvPacketStartBarrier(MediaGraph& graph,
                                                      MediaNodeId videoMux,
                                                      MediaNodeId audioMux,
                                                      const MediaRealtimeAvStartBarrierPlan& barrierPlan,
                                                      const MediaRealtimeEdgePolicySet& edgePolicies)
{
    const MediaNodeId barrier = graph.addNode(MediaNodeKind::AvPacketStartBarrier,
                                              "realtime.av.start_barrier",
                                              "Realtime A/V RTP packet start barrier");
    if (!barrier.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError("MediaRealtimeRtpTranscodeGraphBuilder failed to add A/V start barrier"));
    }

    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, barrier, "av_start_barrier.expect_video", barrierPlan.expectVideo ? "1" : "0"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, barrier, "av_start_barrier.expect_audio", barrierPlan.expectAudio ? "1" : "0"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, barrier, "av_start_barrier.require_video_key_frame", barrierPlan.requireVideoKeyFrame ? "1" : "0"); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, barrier, "video_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, barrier, "video_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, barrier, "video_packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, barrier, "video_packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, barrier, "audio_codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, barrier, "audio_codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, barrier, "audio_packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, barrier, "audio_packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, barrier, "video_codec", videoMux, "codec", "realtime.av.start_barrier.video_codec -> video_mux.codec", edgePolicies.metadata); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, barrier, "video_packet", videoMux, "packet", "realtime.av.start_barrier.video_packet -> video_mux.packet", edgePolicies.videoMux); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, barrier, "audio_codec", audioMux, "codec", "realtime.av.start_barrier.audio_codec -> audio_mux.codec", edgePolicies.metadata); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, barrier, "audio_packet", audioMux, "packet", "realtime.av.start_barrier.audio_packet -> audio_mux.packet", edgePolicies.audioMux); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    return ::media::Result<MediaNodeId>::success(barrier);
}

} // namespace

::media::Status MediaRealtimeRtpTranscodeGraphBuilder::validate(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    if (!plan) {
        return ::media::Status::failure(plan.error());
    }
    return ::media::Status::success();
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    auto realtimePlan = MediaRealtimeRtpTranscodePlanner::plan(options);
    if (!realtimePlan) {
        return ::media::Result<MediaGraph>::failure(realtimePlan.error());
    }
    return build(std::move(realtimePlan).value());
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    MediaRealtimeRtpTranscodePlan plan)
{
    MediaGraph graph;

    const MediaNodeKind inputKind = plan.inputType == RealtimeInputType::RtpPort
        ? MediaNodeKind::RawRtpInput
        : MediaNodeKind::RealtimeInput;
    const bool includeAudio = branchEnabled(plan.audioPlan);
    MediaNodeId videoMux = MediaNodeId::invalid();
    MediaNodeId audioMux = MediaNodeId::invalid();
    MediaNodeId avStartBarrier = MediaNodeId::invalid();

    if (separateRtpOutput(plan)) {
        const MediaNodeId videoOutput = graph.addNode(MediaNodeKind::RtpOutput,
                                                      "realtime.video.rtp.output",
                                                      "Realtime video RTP output context");
        videoMux = graph.addNode(MediaNodeKind::RtpMux,
                                 "realtime.video.rtp.mux",
                                 "Realtime video RTP mux");
        const MediaNodeId audioOutput = includeAudio
            ? graph.addNode(MediaNodeKind::RtpOutput,
                            "realtime.audio.rtp.output",
                            "Realtime audio RTP output context")
            : MediaNodeId::invalid();
        audioMux = includeAudio
            ? graph.addNode(MediaNodeKind::RtpMux,
                            "realtime.audio.rtp.mux",
                            "Realtime audio RTP mux")
            : MediaNodeId::invalid();
        const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter,
                                              "realtime.sdp.writer",
                                              "Realtime SDP writer");

        if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, videoOutput, plan.videoOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, videoMux, plan.videoOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (includeAudio) {
            if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, audioOutput, plan.audioOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
            if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, audioMux, plan.audioOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
        }
        if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, plan.sdp); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaRealtimeOptionApplier::applyMuxOptions(graph, videoMux, plan.videoMux); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (includeAudio) {
            if (auto status = MediaRealtimeOptionApplier::applyMuxOptions(graph, audioMux, plan.audioMux); !status) return ::media::Result<MediaGraph>::failure(status.error());
        }

        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, sdp, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = addRtpOutputChain(graph, videoOutput, videoMux, sdp, MediaStreamKind::Video, plan.edgePolicies); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (includeAudio) {
            if (auto status = addRtpOutputChain(graph, audioOutput, audioMux, sdp, MediaStreamKind::Audio, plan.edgePolicies); !status) return ::media::Result<MediaGraph>::failure(status.error());
            auto barrier = addAvPacketStartBarrier(graph, videoMux, audioMux, plan.avStartBarrier, plan.edgePolicies);
            if (!barrier) {
                return ::media::Result<MediaGraph>::failure(barrier.error());
            }
            avStartBarrier = barrier.value();
        }
    } else {
        FileOutputSegmentOptions outputOptions;
        outputOptions.prefix = "realtime.mpegts";
        outputOptions.outputUrl = plan.muxedOutput.url;
        outputOptions.outputFormat = plan.muxedOutput.format;
        outputOptions.expectVideo = plan.videoMux.expectVideo;
        outputOptions.expectAudio = plan.videoMux.expectAudio;
        outputOptions.queues = plan.queues;
        auto output = MediaOutputSegmentBuilder::buildFileMuxOutput(graph, outputOptions);
        if (!output) {
            return ::media::Result<MediaGraph>::failure(output.error());
        }
        videoMux = output.value().mux;
        audioMux = includeAudio ? output.value().mux : MediaNodeId::invalid();
    }

    const bool isolateRawRtpAudio = plan.useIsolatedAudioInput;
    auto videoInputChain = addRealtimePacketInputChain(graph,
                                                       inputKind,
                                                       isolateRawRtpAudio ? "realtime.video.input" : "realtime.input",
                                                       "Realtime media input",
                                                       plan.input,
                                                       plan.queues,
                                                       plan.edgePolicies);
    if (!videoInputChain) {
        return ::media::Result<MediaGraph>::failure(videoInputChain.error());
    }

    RealtimePacketInputChain audioInputChain;
    if (isolateRawRtpAudio) {
        auto audioInput = addRealtimePacketInputChain(graph,
                                                      MediaNodeKind::RawRtpInput,
                                                      "realtime.audio.input",
                                                      "Realtime audio RTP input",
                                                      plan.audioInput,
                                                      plan.queues,
                                                      plan.edgePolicies);
        if (!audioInput) {
            return ::media::Result<MediaGraph>::failure(audioInput.error());
        }
        audioInputChain = audioInput.value();
    }

    MediaNodeId videoPacketSourceNode = videoInputChain.value().packetSelect.split;
    std::string videoPacketSourcePort = "video";
    if (plan.inputType == RealtimeInputType::RtpPort) {
        auto normalized = addRawRtpPacketNormalize(graph,
                                                    videoInputChain.value().input,
                                                    videoInputChain.value().packetSelect.split,
                                                    plan.videoPlan.sourceStreamIndex,
                                                    plan.edgePolicies);
        if (!normalized) {
            return ::media::Result<MediaGraph>::failure(normalized.error());
        }
        videoPacketSourceNode = normalized.value();
        videoPacketSourcePort = "packet";
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "realtime.video";
    videoOptions.plan = std::move(plan.videoPlan);
    videoOptions.parameters = plan.videoParameters;
    videoOptions.queues = plan.queues;
    videoOptions.edgePolicies = plan.edgePolicies;
    videoOptions.inputStartRequiresKeyFrame = plan.videoInputStartRequiresKeyFrame;
    videoOptions.formatSourceNode = videoInputChain.value().input;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = videoPacketSourceNode;
    videoOptions.packetSourcePort = videoPacketSourcePort;
    videoOptions.muxNode = avStartBarrier.isValid() ? avStartBarrier : videoMux;
    videoOptions.muxCodecPort = avStartBarrier.isValid() ? "video_codec" : "codec";
    videoOptions.muxPacketPort = avStartBarrier.isValid() ? "video_packet" : "packet";
    auto video = MediaVideoBranchSegmentBuilder::buildIfPlanned(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }
    if (!video.value()) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("MediaRealtimeRtpTranscodeGraphBuilder no video branch was built"));
    }

    if (includeAudio) {
        MediaAudioBranchSegmentOptions audioOptions;
        audioOptions.prefix = "realtime.audio";
        audioOptions.plan = std::move(plan.audioPlan);
        audioOptions.parameters = plan.audioParameters;
        audioOptions.queues = plan.queues;
        audioOptions.edgePolicies = plan.edgePolicies;
        audioOptions.formatSourceNode = isolateRawRtpAudio
            ? audioInputChain.input
            : videoInputChain.value().input;
        audioOptions.formatSourcePort = "format";
        audioOptions.packetSourceNode = isolateRawRtpAudio
            ? audioInputChain.packetSelect.split
            : videoInputChain.value().packetSelect.split;
        audioOptions.packetSourcePort = "audio";
        audioOptions.muxNode = avStartBarrier.isValid() ? avStartBarrier : audioMux;
        audioOptions.muxCodecPort = avStartBarrier.isValid() ? "audio_codec" : "codec";
        audioOptions.muxPacketPort = avStartBarrier.isValid() ? "audio_packet" : "packet";
        auto audio = MediaAudioBranchSegmentBuilder::buildIfPlanned(graph, audioOptions);
        if (!audio) {
            return ::media::Result<MediaGraph>::failure(audio.error());
        }
        if (!audio.value()) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported("MediaRealtimeRtpTranscodeGraphBuilder no audio branch was built"));
        }
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
