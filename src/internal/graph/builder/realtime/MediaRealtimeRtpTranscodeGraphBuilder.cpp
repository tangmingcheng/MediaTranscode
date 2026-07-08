#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

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
                                                      const MediaGraphQueueParameters& queues)
{
    const MediaNodeId normalize = graph.addNode(MediaNodeKind::PacketNormalize,
                                                "realtime.raw_rtp.normalize",
                                                "Raw RTP packet normalize");
    if (!normalize.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError("MediaRealtimeRtpTranscodeGraphBuilder failed to add raw RTP packet normalize node"));
    }
    if (auto status = MediaGraphBuildSupport::setPacketStreamOptions(graph,
                                                                     owner,
                                                                     normalize,
                                                                     MediaStreamKind::Video,
                                                                     sourceStreamIndex); !status) {
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

    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, formatSource, "format", normalize, "format", "realtime.raw_rtp.format -> normalize.format", MediaGraphBuildSupport::blockingQueuePolicy(queues.metadata)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, packetSource, "video", normalize, "packet", "realtime.raw_rtp.packet -> normalize.packet", MediaGraphBuildSupport::blockingQueuePolicy(queues.packet)); !status) return ::media::Result<MediaNodeId>::failure(status.error());

    return ::media::Result<MediaNodeId>::success(normalize);
}

::media::Result<void> addRtpOutputChain(MediaGraph& graph,
                                        MediaNodeId output,
                                        MediaNodeId mux,
                                        MediaNodeId sdp,
                                        MediaStreamKind streamKind,
                                        const MediaEdgePolicy& edgePolicy)
{
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, output, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "codec", streamKind, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "packet", streamKind, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, output, "format", mux, "format", "realtime.rtp.output.format -> realtime.rtp.mux.format", edgePolicy); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, mux, "format", sdp, "format", "realtime.rtp.mux.format -> realtime.sdp.writer.format", edgePolicy, false);
}

bool branchEnabled(const MediaAudioPipelinePlan& plan) noexcept
{
    return plan.enabled && plan.branchMode != MediaBranchMode::Drop;
}

bool separateRtpOutput(const MediaRealtimeRtpTranscodePlan& plan) noexcept
{
    return plan.outputLayout == RealtimeOutputStreamLayout::SeparateStreams;
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
    MediaRealtimeRtpTranscodePlan plan = std::move(realtimePlan).value();

    MediaGraph graph;

    const MediaNodeKind inputKind = plan.inputType == RealtimeInputType::RtpPort
        ? MediaNodeKind::RawRtpInput
        : MediaNodeKind::RealtimeInput;
    const MediaNodeId input = graph.addNode(inputKind,
                                            "realtime.input",
                                            "Realtime media input");
    const bool includeAudio = branchEnabled(plan.audioPlan);
    MediaNodeId videoMux = MediaNodeId::invalid();
    MediaNodeId audioMux = MediaNodeId::invalid();

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, plan.input); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = addRealtimeInputPorts(graph, input); !status) return ::media::Result<MediaGraph>::failure(status.error());

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
        if (auto status = addRtpOutputChain(graph, videoOutput, videoMux, sdp, MediaStreamKind::Video, plan.edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (includeAudio) {
            if (auto status = addRtpOutputChain(graph, audioOutput, audioMux, sdp, MediaStreamKind::Audio, plan.edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());
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

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = "realtime";
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = plan.queues;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    MediaNodeId videoPacketSourceNode = packetSelect.value().split;
    std::string videoPacketSourcePort = "video";
    if (plan.inputType == RealtimeInputType::RtpPort) {
        auto normalized = addRawRtpPacketNormalize(graph,
                                                   input,
                                                   packetSelect.value().split,
                                                   plan.videoPlan.sourceStreamIndex,
                                                   plan.queues);
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
    videoOptions.formatSourceNode = input;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = videoPacketSourceNode;
    videoOptions.packetSourcePort = videoPacketSourcePort;
    videoOptions.muxNode = videoMux;
    videoOptions.muxCodecPort = "codec";
    videoOptions.muxPacketPort = "packet";
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
        audioOptions.formatSourceNode = input;
        audioOptions.formatSourcePort = "format";
        audioOptions.packetSourceNode = packetSelect.value().split;
        audioOptions.packetSourcePort = "audio";
        audioOptions.muxNode = audioMux;
        audioOptions.muxCodecPort = "codec";
        audioOptions.muxPacketPort = "packet";
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
