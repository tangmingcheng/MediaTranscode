#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeEdgePolicy.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

std::string effectiveInputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.input.url.empty() ? options.input.url : options.inputUrl;
}

std::string effectiveOutputHost(const MediaRealtimeGraphBuilderOptions& options)
{
    return options.output.host.empty() ? std::string("127.0.0.1") : options.output.host;
}

bool includeVideoBranch(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    return options.includeVideo && options.parameters.execution.includeVideo;
}

bool isValidRtpPort(std::size_t port) noexcept
{
    return port > 0 && port <= 65534 && (port % 2) == 0;
}

::media::Result<MediaPipelinePlannerOptions> buildPlannerOptions(
    const MediaRealtimeGraphBuilderOptions& options)
{
    const MediaVideoTranscodeParameters& video = options.parameters.video;
    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Result<MediaPipelinePlannerOptions>::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video width and height must be specified together"));
    }

    MediaPipelinePlannerOptions plannerOptions;
    plannerOptions.includeVideo = includeVideoBranch(options);
    plannerOptions.allowPacketCopy = false;
    plannerOptions.outputPath = effectiveOutputHost(options);
    plannerOptions.outputCodecName = video.codecName.empty() ? std::string("h264") : video.codecName;
    plannerOptions.targetWidth = video.width.value_or(0);
    plannerOptions.targetHeight = video.height.value_or(0);
    plannerOptions.filterRequired = true;
    plannerOptions.preferGpu = !options.parameters.execution.disableHardware;
    plannerOptions.allowSoftwareFallback = true;
    plannerOptions.requireRuntimeAvailability = true;
    plannerOptions.preferredHardware = plannerOptions.preferGpu ? "auto" : "software";
    plannerOptions.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    plannerOptions.rtspTransport = options.input.rtspTransport;
    plannerOptions.openTimeoutMs = options.input.openTimeoutMs;
    plannerOptions.readTimeoutMs = options.input.readTimeoutMs;
    plannerOptions.analyzeDurationUs = options.input.analyzeDurationUs;
    plannerOptions.probeSizeBytes = options.input.probeSizeBytes;
    plannerOptions.lowLatency = options.input.lowLatency;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
}

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
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (effectiveInputUrl(options).empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimeRtpTranscodeGraphBuilder requires input URL"));
    }
    if (!includeVideoBranch(options)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimeRtpTranscodeGraphBuilder requires video branch"));
    }
    if (!isValidRtpPort(options.output.basePort)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP output base port must be an even port in range 1..65534"));
    }
    if (options.parameters.queues.metadata == 0 ||
        options.parameters.queues.packet == 0 ||
        options.parameters.queues.frame == 0 ||
        options.parameters.queues.mux == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP queue capacities must be greater than 0"));
    }
    return ::media::Status::success();
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto validation = validate(options); !validation) {
        return ::media::Result<MediaGraph>::failure(validation.error());
    }

    auto plannerOptions = buildPlannerOptions(options);
    if (!plannerOptions) {
        return ::media::Result<MediaGraph>::failure(plannerOptions.error());
    }
    auto plannedVideo = MediaPipelinePlanner::planVideoTranscodeRealtimeUrl(
        effectiveInputUrl(options),
        std::move(plannerOptions).value());
    if (!plannedVideo) {
        return ::media::Result<MediaGraph>::failure(plannedVideo.error());
    }
    MediaPipelinePlan videoPlan = std::move(plannedVideo).value();

    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = MediaRealtimeEdgePolicy::make(options);
    const MediaGraphQueueParameters& queues = options.parameters.queues;

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

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, output, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, mux, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, mux, MediaTranscodeOptionKey::MuxExpectVideo, "1"); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, mux, MediaTranscodeOptionKey::MuxExpectAudio, "0"); !status) return ::media::Result<MediaGraph>::failure(status.error());

    if (auto status = addRealtimeInputPorts(graph, input); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addRtpOutputChain(graph, output, mux, sdp, edgePolicy); !status) return ::media::Result<MediaGraph>::failure(status.error());

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = "realtime";
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = queues;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "realtime.video";
    videoOptions.plan = std::move(videoPlan);
    videoOptions.parameters = options.parameters.video;
    videoOptions.parameters.bFrames = 0;
    videoOptions.queues = queues;
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
