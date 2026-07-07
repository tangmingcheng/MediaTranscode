#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
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

::media::Result<void> addRtpOutputChain(MediaGraph& graph,
                                        MediaNodeId output,
                                        MediaNodeId mux,
                                        MediaNodeId sdp,
                                        const MediaEdgePolicy& edgePolicy)
{
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, output, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "codec", MediaStreamKind::Any, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, mux, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph, owner, mux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, sdp, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph, owner, output, "format", mux, "format", "realtime.rtp.output.format -> realtime.rtp.mux.format", edgePolicy); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph, owner, mux, "format", sdp, "format", "realtime.rtp.mux.format -> realtime.sdp.writer.format", edgePolicy, false);
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

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput,
                                            "realtime.input",
                                            "Realtime media input");
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput,
                                             "realtime.rtp.output",
                                             "Realtime RTP output context");
    const MediaNodeId mux = graph.addNode(MediaNodeKind::RtpMux,
                                          "realtime.rtp.mux",
                                          "Realtime RTP mux");
    const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter,
                                          "realtime.sdp.writer",
                                          "Realtime SDP writer");

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, plan.input); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, output, plan.output); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, mux, plan.output); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, plan.sdp); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyMuxOptions(graph, mux, plan.mux); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = addRealtimeInputPorts(graph, input); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addRtpOutputChain(graph, output, mux, sdp, plan.edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = "realtime";
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = plan.queues;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "realtime.video";
    videoOptions.plan = std::move(plan.videoPlan);
    videoOptions.parameters = plan.videoParameters;
    videoOptions.queues = plan.queues;
    videoOptions.formatSourceNode = input;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = packetSelect.value().split;
    videoOptions.packetSourcePort = "video";
    videoOptions.muxNode = mux;
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

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
