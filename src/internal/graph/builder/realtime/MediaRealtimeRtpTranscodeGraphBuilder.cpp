#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeEdgePolicy.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpUrl.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

std::string effectiveInputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.input.url.empty() ? options.input.url : options.inputUrl;
}

std::string effectiveSdpPath(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.output.sdpPath.empty() ? options.output.sdpPath : options.sdpPath;
}

std::string effectiveOutputHost(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.output.host.empty() ? options.output.host : std::string("127.0.0.1");
}

bool isValidRtpPort(std::size_t port) noexcept
{
    return port > 0 && port <= 65534 && (port % 2) == 0;
}

bool completeHint(const MediaRtpCodecHint& hint) noexcept
{
    return (hint.streamKind == MediaStreamKind::Video || hint.streamKind == MediaStreamKind::Audio) &&
           !hint.codecName.empty() &&
           hint.payloadType >= 0 &&
           hint.payloadType <= 127 &&
           hint.clockRate > 0;
}

bool hasCompleteHint(const MediaRealtimeInputConfig& input) noexcept
{
    for (const MediaRtpCodecHint& hint : input.codecHints) {
        if (!completeHint(hint)) {
            return false;
        }
    }
    return !input.codecHints.empty();
}

bool hasCodecHintsWithoutSdp(const MediaRealtimeInputConfig& input) noexcept
{
    return input.mode == MediaRealtimeInputMode::RawRtp &&
           input.sdpText.empty() &&
           input.sdpPath.empty() &&
           !input.codecHints.empty();
}

bool includeVideoBranch(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    return options.includeVideo && options.parameters.execution.includeVideo;
}

bool includeAudioBranch(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    return options.includeAudio && options.parameters.execution.includeAudio;
}

int hintedStreamIndex(MediaStreamKind kind, const MediaRealtimeInputConfig& input) noexcept
{
    int index = 0;
    for (const MediaRtpCodecHint& hint : input.codecHints) {
        if (hint.streamKind == kind) {
            return index;
        }
        ++index;
    }
    return invalidMediaStreamIndex;
}

int videoSourceStreamIndex(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    if (options.input.videoStreamIndex >= 0) {
        return options.input.videoStreamIndex;
    }
    if (hasCodecHintsWithoutSdp(options.input)) {
        return hintedStreamIndex(MediaStreamKind::Video, options.input);
    }
    return invalidMediaStreamIndex;
}

int audioSourceStreamIndex(const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    if (options.input.audioStreamIndex >= 0) {
        return options.input.audioStreamIndex;
    }
    if (hasCodecHintsWithoutSdp(options.input)) {
        return hintedStreamIndex(MediaStreamKind::Audio, options.input);
    }
    return invalidMediaStreamIndex;
}

std::size_t rtpPortFor(MediaStreamKind kind, const MediaRealtimeGraphBuilderOptions& options) noexcept
{
    if (kind == MediaStreamKind::Video) {
        return options.output.videoRtpPort != 0 ? options.output.videoRtpPort : options.output.basePort;
    }
    if (kind == MediaStreamKind::Audio) {
        if (options.output.audioRtpPort != 0) {
            return options.output.audioRtpPort;
        }
        return includeVideoBranch(options) ? options.output.basePort + 2 : options.output.basePort;
    }
    return options.output.basePort;
}

MediaRealtimeGraphBuilderOptions outputOptionsFor(MediaStreamKind kind,
                                                  const MediaRealtimeGraphBuilderOptions& options)
{
    MediaRealtimeGraphBuilderOptions copy = options;
    copy.output.basePort = rtpPortFor(kind, options);
    copy.outputUrl = "rtp://" + effectiveOutputHost(copy) + ":" + std::to_string(copy.output.basePort);
    return copy;
}

MediaPipelineStagePlan softwareStage(MediaPipelineStageRole role,
                                     std::string component,
                                     std::string ffmpegName)
{
    MediaPipelineStagePlan stage;
    stage.role = role;
    stage.componentName = std::move(component);
    stage.ffmpegName = std::move(ffmpegName);
    stage.codecName = stage.ffmpegName;
    stage.filterName = role == MediaPipelineStageRole::Filter ? "null" : std::string{};
    stage.frameKind = MediaHardwareFrameKind::Software;
    stage.deviceKind = MediaHardwareDeviceKind::None;
    stage.available = true;
    return stage;
}

MediaPipelinePlan defaultRealtimeVideoPlan(const MediaRealtimeGraphBuilderOptions& options)
{
    MediaPipelinePlan plan;
    plan.enabled = includeVideoBranch(options);
    plan.branchMode = plan.enabled ? MediaBranchMode::TranscodeFrame : MediaBranchMode::Drop;
    plan.sourceStreamIndex = videoSourceStreamIndex(options);
    plan.inputPath = effectiveInputUrl(options);
    plan.outputPath = effectiveOutputHost(options);
    plan.outputCodecName = !options.parameters.video.codecName.empty()
        ? options.parameters.video.codecName
        : std::string("h264");
    plan.diagnosticLogEnabled = options.parameters.execution.diagnosticLogEnabled;
    plan.reason = "realtime low latency default video transcode plan";
    plan.selected.label = "realtime-software";
    plan.selected.available = true;
    plan.selected.reason = plan.reason;
    plan.selected.decoder = softwareStage(MediaPipelineStageRole::Decoder, "software", "auto");
    plan.selected.filter = softwareStage(MediaPipelineStageRole::Filter, "software", "null");
    plan.selected.encoder = softwareStage(MediaPipelineStageRole::Encoder, "software", plan.outputCodecName);
    return plan;
}

MediaAudioPipelinePlan defaultRealtimeAudioPlan(const MediaRealtimeGraphBuilderOptions& options)
{
    MediaAudioPipelinePlan plan;
    plan.enabled = includeAudioBranch(options);
    plan.branchMode = plan.enabled ? MediaBranchMode::TranscodeFrame : MediaBranchMode::Drop;
    plan.sourceStreamIndex = audioSourceStreamIndex(options);
    plan.targetCodecName = !options.parameters.audio.codecName.empty()
        ? options.parameters.audio.codecName
        : std::string("aac");
    plan.targetEncoderName = plan.targetCodecName;
    plan.reason = "realtime low latency default audio transcode plan";
    return plan;
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
                                                        false);
}

::media::Result<void> addSdpWriterPorts(MediaGraph& graph, MediaNodeId sdp)
{
    return MediaGraphBuildSupport::addInputPortChecked(graph,
                                                       owner,
                                                       sdp,
                                                       "format",
                                                       MediaStreamKind::Metadata,
                                                       MediaEdgeKind::Metadata,
                                                       MediaPayloadKind::FormatContext,
                                                       true,
                                                       true);
}

::media::Result<void> addRtpOutputPorts(MediaGraph& graph,
                                        MediaNodeId output,
                                        MediaNodeId mux,
                                        MediaNodeId sdp,
                                        const MediaEdgePolicy& edgePolicy,
                                        const std::string& prefix)
{
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   output,
                                                                   "format",
                                                                   MediaStreamKind::Metadata,
                                                                   MediaEdgeKind::Metadata,
                                                                   MediaPayloadKind::FormatContext,
                                                                   true,
                                                                   true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  mux,
                                                                  "format",
                                                                  MediaStreamKind::Metadata,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::FormatContext,
                                                                  true,
                                                                  false); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  mux,
                                                                  "codec",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::Metadata,
                                                                  MediaPayloadKind::Unknown,
                                                                  true,
                                                                  true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph,
                                                                  owner,
                                                                  mux,
                                                                  "packet",
                                                                  MediaStreamKind::Any,
                                                                  MediaEdgeKind::Unknown,
                                                                  MediaPayloadKind::Packet,
                                                                  true,
                                                                  true); !status) return status;
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(graph,
                                                                   owner,
                                                                   mux,
                                                                   "format",
                                                                   MediaStreamKind::Metadata,
                                                                   MediaEdgeKind::Metadata,
                                                                   MediaPayloadKind::FormatContext,
                                                                   true,
                                                                   true); !status) return status;
    if (auto status = MediaGraphBuildSupport::connectChecked(graph,
                                                            owner,
                                                            output,
                                                            "format",
                                                            mux,
                                                            "format",
                                                            prefix + ".rtp.output.format -> rtp.mux.format",
                                                            edgePolicy); !status) return status;
    return MediaGraphBuildSupport::connectChecked(graph,
                                                  owner,
                                                  mux,
                                                  "format",
                                                  sdp,
                                                  "format",
                                                  prefix + ".rtp.mux.format -> realtime.sdp.writer.format",
                                                  edgePolicy,
                                                  false);
}

::media::Result<MediaNodeId> addRtpOutputChain(MediaGraph& graph,
                                               MediaNodeId sdp,
                                               const MediaEdgePolicy& edgePolicy,
                                               const MediaRealtimeGraphBuilderOptions& options,
                                               MediaStreamKind kind,
                                               const std::string& prefix)
{
    const MediaNodeId output = graph.addNode(MediaNodeKind::RtpOutput,
                                             prefix + ".rtp.output",
                                             prefix + " RTP output context");
    const MediaNodeId mux = graph.addNode(MediaNodeKind::RtpMux,
                                          prefix + ".rtp.mux",
                                          prefix + " RTP mux");
    MediaRealtimeGraphBuilderOptions outputOptions = outputOptionsFor(kind, options);
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, output, outputOptions); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, mux, outputOptions); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                                   owner,
                                                                   mux,
                                                                   MediaTranscodeOptionKey::MuxExpectVideo,
                                                                   kind == MediaStreamKind::Video ? "1" : "0"); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph,
                                                                   owner,
                                                                   mux,
                                                                   MediaTranscodeOptionKey::MuxExpectAudio,
                                                                   kind == MediaStreamKind::Audio ? "1" : "0"); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = addRtpOutputPorts(graph, output, mux, sdp, edgePolicy, prefix); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    return ::media::Result<MediaNodeId>::success(mux);
}

} // namespace

::media::Status MediaRealtimeRtpTranscodeGraphBuilder::validate(
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (effectiveInputUrl(options).empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimeRtpTranscodeGraphBuilder requires input URL"));
    }
    if (!includeVideoBranch(options) && !includeAudioBranch(options)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimeRtpTranscodeGraphBuilder requires audio or video branch"));
    }
    if (options.input.mode == MediaRealtimeInputMode::RawRtp &&
        options.input.sdpText.empty() &&
        options.input.sdpPath.empty() &&
        !hasCompleteHint(options.input)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP input requires SDP or complete codec hints"));
    }
    if (hasCodecHintsWithoutSdp(options.input) && !parseRealtimeRtpUrlPort(effectiveInputUrl(options))) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Raw RTP codec hints require input URL with a valid RTP port"));
    }
    if (includeVideoBranch(options) && videoSourceStreamIndex(options) < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP video branch requires input.videoStreamIndex or video codec hint"));
    }
    if (includeAudioBranch(options) && audioSourceStreamIndex(options) < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Realtime RTP audio branch requires input.audioStreamIndex or audio codec hint"));
    }
    if (!isValidRtpPort(options.output.basePort)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP output base port must be an even port in range 1..65534"));
    }
    if (includeVideoBranch(options) && !isValidRtpPort(rtpPortFor(MediaStreamKind::Video, options))) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP output video port must be an even port in range 1..65534"));
    }
    if (includeAudioBranch(options) && !isValidRtpPort(rtpPortFor(MediaStreamKind::Audio, options))) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP output audio port must be an even port in range 1..65534"));
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

    MediaGraph graph;
    const MediaEdgePolicy edgePolicy = MediaRealtimeEdgePolicy::make(options);
    const MediaGraphQueueParameters& queues = options.parameters.queues;

    const MediaNodeId input = graph.addNode(MediaNodeKind::RealtimeInput,
                                            "realtime.input",
                                            "Realtime media input");
    const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter,
                                          "realtime.sdp.writer",
                                          "Realtime SDP writer");

    if (auto status = MediaRealtimeOptionApplier::applyInputOptions(graph, input, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, options); !status) return ::media::Result<MediaGraph>::failure(status.error());
    const int expectedSdpContexts = (includeVideoBranch(options) ? 1 : 0) + (includeAudioBranch(options) ? 1 : 0);
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, sdp, "sdp.expected_contexts", std::to_string(expectedSdpContexts)); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addRealtimeInputPorts(graph, input); !status) return ::media::Result<MediaGraph>::failure(status.error());
    if (auto status = addSdpWriterPorts(graph, sdp); !status) return ::media::Result<MediaGraph>::failure(status.error());

    MediaNodeId videoMux = MediaNodeId::invalid();
    MediaNodeId audioMux = MediaNodeId::invalid();
    if (includeVideoBranch(options)) {
        auto chain = addRtpOutputChain(graph, sdp, edgePolicy, options, MediaStreamKind::Video, "realtime.video");
        if (!chain) {
            return ::media::Result<MediaGraph>::failure(chain.error());
        }
        videoMux = chain.value();
    }
    if (includeAudioBranch(options)) {
        auto chain = addRtpOutputChain(graph, sdp, edgePolicy, options, MediaStreamKind::Audio, "realtime.audio");
        if (!chain) {
            return ::media::Result<MediaGraph>::failure(chain.error());
        }
        audioMux = chain.value();
    }

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = "realtime";
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = queues;
    auto packetSelect = MediaPacketSelectSegmentBuilder::buildDemuxStreamSplit(graph, packetSelectOptions);
    if (!packetSelect) {
        return ::media::Result<MediaGraph>::failure(packetSelect.error());
    }

    bool builtBranch = false;
    MediaPipelinePlan videoPlan = defaultRealtimeVideoPlan(options);
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
    videoOptions.muxNode = videoMux;
    auto video = MediaVideoBranchSegmentBuilder::buildIfPlanned(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }
    builtBranch = builtBranch || video.value();

    MediaAudioPipelinePlan audioPlan = defaultRealtimeAudioPlan(options);
    MediaAudioBranchSegmentOptions audioOptions;
    audioOptions.prefix = "realtime.audio";
    audioOptions.plan = std::move(audioPlan);
    audioOptions.parameters = options.parameters.audio;
    audioOptions.queues = queues;
    audioOptions.formatSourceNode = input;
    audioOptions.formatSourcePort = "format";
    audioOptions.packetSourceNode = packetSelect.value().split;
    audioOptions.packetSourcePort = "audio";
    audioOptions.muxNode = audioMux;
    auto audio = MediaAudioBranchSegmentBuilder::buildIfPlanned(graph, audioOptions);
    if (!audio) {
        return ::media::Result<MediaGraph>::failure(audio.error());
    }
    builtBranch = builtBranch || audio.value();

    if (!builtBranch) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported("MediaRealtimeRtpTranscodeGraphBuilder no media branches were built"));
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

} // namespace media::ffmpeg::graph
