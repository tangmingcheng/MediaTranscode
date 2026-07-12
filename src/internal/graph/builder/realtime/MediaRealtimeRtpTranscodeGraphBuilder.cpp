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

    if (inputKind == MediaNodeKind::RawRtpInput) {
        if (!inputPlan.rtpDepacketizer) {
            return ::media::Result<RealtimePacketInputChain>::failure(
                ::media::ErrorInfo::invalidArgument("raw RTP input requires depacketizer plan"));
        }
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                graph, owner, input, "clock", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                MediaPayloadKind::GraphEvent, false, true); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                graph, owner, input, "event", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                MediaPayloadKind::GraphEvent, false, true); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        RealtimePacketInputChain chain;
        chain.input = input;
        chain.packetSelect.split = input;
        return ::media::Result<RealtimePacketInputChain>::success(chain);
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

::media::Result<MediaNodeId> addRtpClockGroup(
    MediaGraph& graph,
    MediaNodeId videoInput,
    MediaNodeId audioInput,
    const MediaAvSyncPlan& avSync,
    std::int64_t videoCnameTimeoutNs,
    std::int64_t audioCnameTimeoutNs,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    if (!avSync.rtp || !avSync.rtp->videoInput.clockRate || !avSync.rtp->audioInput.clockRate ||
        !avSync.rtp->input.senderReportTimeoutNs || !avSync.rtp->input.maximumExtrapolationNs ||
        !avSync.rtp->input.maximumSenderReportSkewNs ||
        !avSync.rtp->input.maximumSenderClockRateErrorPpm ||
        !avSync.rtp->input.maximumSenderClockResidualNs ||
        videoCnameTimeoutNs <= 0 || audioCnameTimeoutNs <= 0) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::invalidArgument("RTP clock group requires a complete planner-owned A/V sync plan"));
    }
    const MediaNodeId group = graph.addNode(MediaNodeKind::RtpClockGroup,
                                            "realtime.rtp.clock_group",
                                            "Realtime RTP source clock group");
    if (!group.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError("failed to add RTP clock group node"));
    }
    const auto set = [&](const char* key, std::string value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, group, key, value);
    };
    if (auto status = set("rtp_clock_group.video_clock_rate", std::to_string(*avSync.rtp->videoInput.clockRate)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.audio_clock_rate", std::to_string(*avSync.rtp->audioInput.clockRate)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.sender_report_timeout_ns", std::to_string(avSync.rtp->input.senderReportTimeoutNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.maximum_extrapolation_ns", std::to_string(avSync.rtp->input.maximumExtrapolationNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.maximum_inter_stream_skew_ns", std::to_string(avSync.rtp->input.maximumSenderReportSkewNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.maximum_sender_clock_residual_ns", std::to_string(avSync.rtp->input.maximumSenderClockResidualNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.video_cname_timeout_ns", std::to_string(videoCnameTimeoutNs)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.audio_cname_timeout_ns", std::to_string(audioCnameTimeoutNs)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.maximum_sender_clock_rate_error_ppm", std::to_string(*avSync.rtp->input.maximumSenderClockRateErrorPpm)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    for (MediaNodeId input : {videoInput, audioInput}) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, input, "rtcp.maximum_extrapolation_ns",
                std::to_string(avSync.rtp->input.maximumExtrapolationNs->nanoseconds())); !status) {
            return ::media::Result<MediaNodeId>::failure(status.error());
        }
    }

    const struct PortSpec {
        const char* name;
    } inputs[] = {{"video_clock"}, {"video_event"}, {"audio_clock"}, {"audio_event"}};
    for (const auto& input : inputs) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, group, input.name, MediaStreamKind::Metadata,
                MediaEdgeKind::Metadata, MediaPayloadKind::GraphEvent, true, false); !status) {
            return ::media::Result<MediaNodeId>::failure(status.error());
        }
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
            graph, owner, group, "clock_group", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata, MediaPayloadKind::GraphEvent, false, true); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    const auto connect = [&](MediaNodeId from,
                             const char* fromPort,
                             const char* toPort,
                             const char* label) {
        return MediaGraphBuildSupport::connectChecked(
            graph, owner, from, fromPort, group, toPort, label, edgePolicies.metadata);
    };
    if (auto status = connect(videoInput, "clock", "video_clock", "video RTP clock -> RTP clock group"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = connect(videoInput, "event", "video_event", "video RTP event -> RTP clock group"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = connect(audioInput, "clock", "audio_clock", "audio RTP clock -> RTP clock group"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = connect(audioInput, "event", "audio_event", "audio RTP event -> RTP clock group"); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    return ::media::Result<MediaNodeId>::success(group);
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
    return MediaRealtimeRtpTranscodePlanner::validateRealtimeRequestNoIo(options);
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::build(
    const MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.input.type || *options.input.type != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::unsupported(
                "URL and MPEG-TS realtime input require preflight() and buildExecutable()"));
    }
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

        if (!plan.avSync || !plan.input.rtpTransport || !plan.audioInput.rtpTransport) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated RTP A/V inputs require A/V sync and transport plans"));
        }
        auto clockGroup = addRtpClockGroup(graph,
                                           videoInputChain.value().input,
                                           audioInputChain.input,
                                           *plan.avSync,
                                           static_cast<std::int64_t>(
                                               plan.input.rtpTransport->cnameTimeoutMs) * 1'000'000,
                                           static_cast<std::int64_t>(
                                               plan.audioInput.rtpTransport->cnameTimeoutMs) * 1'000'000,
                                           plan.edgePolicies);
        if (!clockGroup) return ::media::Result<MediaGraph>::failure(clockGroup.error());
    }

    MediaNodeId videoPacketSourceNode = videoInputChain.value().packetSelect.split;
    std::string videoPacketSourcePort = plan.inputType == RealtimeInputType::RtpPort ? "packet" : "video";

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
    videoOptions.normalizePacketCopy = plan.videoPacketCopyNormalizationRequired;
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
        if (isolateRawRtpAudio) audioOptions.packetSourcePort = "packet";
        audioOptions.normalizeInputPackets = plan.audioPacketNormalizationRequired;
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

::media::Result<MediaRealtimeExecutableGraph> MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
    MediaRealtimeTranscodePreflight preflight)
{
    const bool requiresPrepared = preflight.plan.inputType != RealtimeInputType::RtpPort;
    if (requiresPrepared && (!preflight.prepared || !preflight.prepared->valid())) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized("realtime executable graph requires prepared input"));
    }
    auto graphResult = build(std::move(preflight.plan));
    if (!graphResult) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(graphResult.error());
    }
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graphResult).value();
    if (requiresPrepared) {
        MediaNodeId inputId = MediaNodeId::invalid();
        for (const MediaNode& node : executable.graph.nodes()) {
            if (node.kind != MediaNodeKind::RealtimeInput) continue;
            if (inputId.isValid()) {
                return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                    ::media::ErrorInfo::invalidArgument("realtime executable graph has duplicate realtime inputs"));
            }
            inputId = node.id;
        }
        if (!inputId.isValid()) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                ::media::ErrorInfo::notInitialized("realtime executable graph has no realtime input node"));
        }
        executable.inputBindings.push_back(
            MediaPreparedRealtimeInputBinding{inputId, std::move(*preflight.prepared)});
    }
    return ::media::Result<MediaRealtimeExecutableGraph>::success(std::move(executable));
}

} // namespace media::ffmpeg::graph
