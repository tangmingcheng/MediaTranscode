#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <string>
#include <optional>
#include <tuple>
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
    bool requiresProtocolClock,
    const std::optional<PacketSelectOutputPlan>& videoOutput,
    const std::optional<PacketSelectOutputPlan>& audioOutput,
    const MediaRealtimeRtpInputNodePlan& inputPlan,
    const MediaGraphQueueParameters& queues,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    if (!videoOutput && !audioOutput) {
        return ::media::Result<RealtimePacketInputChain>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime input requires at least one planned packet output"));
    }
    for (const PacketSelectOutputPlan* output : {
             videoOutput ? &*videoOutput : nullptr,
             audioOutput ? &*audioOutput : nullptr}) {
        if (output &&
            (output->sourceStreamIndex < 0 ||
             (output->edgeKind != MediaEdgeKind::InputPacket &&
              output->edgeKind != MediaEdgeKind::EncodedPacket))) {
            return ::media::Result<RealtimePacketInputChain>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "realtime input requires complete packet output plans"));
        }
    }
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
                graph, owner, input, "clock", MediaStreamKind::Metadata, MediaEdgeKind::Event,
                MediaPayloadKind::GraphEvent, false, true); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                graph, owner, input, "event", MediaStreamKind::Metadata, MediaEdgeKind::Event,
                MediaPayloadKind::GraphEvent, false, true); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        if (videoOutput.has_value() == audioOutput.has_value()) {
            return ::media::Result<RealtimePacketInputChain>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP input requires exactly one planned packet output"));
        }
        const PacketSelectOutputPlan& output = videoOutput
            ? *videoOutput : *audioOutput;
        const MediaStreamKind stream = videoOutput
            ? MediaStreamKind::Video : MediaStreamKind::Audio;
        if (auto status = MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
                graph, owner, input, "packet", stream, output.edgeKind,
                MediaPayloadKind::Packet, true, true,
                MediaGraphBuildSupport::streamIndexDescriptor(
                    stream, output.sourceStreamIndex)); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(
                status.error());
        }
        RealtimePacketInputChain chain;
        chain.input = input;
        chain.packetSelect.split = input;
        return ::media::Result<RealtimePacketInputChain>::success(chain);
    }

    if (inputPlan.mpegTs) {
        const MediaNodeId demux = graph.addNode(MediaNodeKind::MpegTsDemux,
                                                prefix + ".mpegts_demux",
                                                "MPEG-TS program-clock demux");
        if (!demux.isValid()) {
            return ::media::Result<RealtimePacketInputChain>::failure(
                ::media::ErrorInfo::internalError("failed to add MPEG-TS demux node"));
        }
        if (auto status = MediaRealtimeOptionApplier::applyMpegTsDemuxOptions(
                graph, demux, *inputPlan.mpegTs); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, demux, "format", MediaStreamKind::Metadata,
                MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        for (const auto& [name, stream, output] : {
                 std::tuple{"video", MediaStreamKind::Video, videoOutput},
                 std::tuple{"audio", MediaStreamKind::Audio, audioOutput}}) {
            if (!output) continue;
            if (auto status = MediaGraphBuildSupport::addOutputPortWithFormatDescriptorChecked(
                    graph, owner, demux, name, stream, output->edgeKind,
                    MediaPayloadKind::Packet, false, true,
                    MediaGraphBuildSupport::streamIndexDescriptor(
                        stream, output->sourceStreamIndex)); !status) {
                return ::media::Result<RealtimePacketInputChain>::failure(
                    status.error());
            }
        }
        if (requiresProtocolClock) {
            if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
                    graph, owner, demux, "clock", MediaStreamKind::Metadata,
                    MediaEdgeKind::Event, MediaPayloadKind::GraphEvent,
                    false, true); !status) {
                return ::media::Result<RealtimePacketInputChain>::failure(
                    status.error());
            }
        }
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, input, "format", demux, "format",
                "realtime.input.format -> mpegts_demux.format", edgePolicies.metadata); !status) {
            return ::media::Result<RealtimePacketInputChain>::failure(status.error());
        }
        RealtimePacketInputChain chain;
        chain.input = input;
        chain.packetSelect.demux = demux;
        chain.packetSelect.split = demux;
        return ::media::Result<RealtimePacketInputChain>::success(chain);
    }

    PacketSelectSegmentOptions packetSelectOptions;
    packetSelectOptions.prefix = prefix;
    packetSelectOptions.formatSourceNode = input;
    packetSelectOptions.formatSourcePort = "format";
    packetSelectOptions.queues = queues;
    packetSelectOptions.edgePolicies = edgePolicies;
    packetSelectOptions.videoOutput = videoOutput;
    packetSelectOptions.audioOutput = audioOutput;
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

bool separateStreamsOutput(const MediaRealtimeRtpTranscodePlan& plan) noexcept
{
    return plan.outputLayout == RealtimeOutputStreamLayout::SeparateStreams;
}

PacketSelectOutputPlan packetOutputPlan(int sourceStreamIndex,
                                        MediaBranchMode branchMode,
                                        bool normalizePacketCopy,
                                        bool synchronized) noexcept
{
    const MediaEdgeKind edgeKind =
        synchronized || branchMode != MediaBranchMode::CopyPacket ||
            normalizePacketCopy
        ? MediaEdgeKind::InputPacket
        : MediaEdgeKind::EncodedPacket;
    return PacketSelectOutputPlan{sourceStreamIndex, edgeKind};
}

::media::Result<MediaNodeId> addRtpClockGroup(
    MediaGraph& graph,
    MediaNodeId videoInput,
    MediaNodeId audioInput,
    const MediaAvSyncPlan& avSync,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    if (!avSync.rtpInput || !avSync.rtpInput->videoInput.clockRate ||
        !avSync.rtpInput->audioInput.clockRate ||
        !avSync.rtpInput->input.senderReportTimeoutNs ||
        !avSync.rtpInput->input.maximumExtrapolationNs ||
        !avSync.rtpInput->input.maximumInterStreamClockOffsetSkewNs ||
        !avSync.rtpInput->input.maximumSenderClockRateErrorPpm ||
        !avSync.rtpInput->input.maximumSenderClockResidualNs ||
        !avSync.rtpInput->input.identityEvidenceTimeoutNs ||
        !mediaRtpCommonEpochPolicyOptionValue(
            avSync.rtpInput->input.commonEpochPolicy) ||
        avSync.rtpInput->input.streamAssociationMode !=
            MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::invalidArgument("RTP clock group requires a complete planner-owned A/V sync plan"));
    }
    const std::int64_t identityEvidenceTimeoutNs =
        avSync.rtpInput->input.identityEvidenceTimeoutNs->nanoseconds();
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
    if (auto status = set("rtp_clock_group.video_clock_rate", std::to_string(*avSync.rtpInput->videoInput.clockRate)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.audio_clock_rate", std::to_string(*avSync.rtpInput->audioInput.clockRate)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.sender_report_timeout_ns", std::to_string(avSync.rtpInput->input.senderReportTimeoutNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.maximum_extrapolation_ns", std::to_string(avSync.rtpInput->input.maximumExtrapolationNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set(
            "rtp_clock_group.maximum_inter_stream_clock_offset_skew_ns",
            std::to_string(avSync.rtpInput->input
                               .maximumInterStreamClockOffsetSkewNs
                               ->nanoseconds()));
        !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = set("rtp_clock_group.maximum_sender_clock_residual_ns", std::to_string(avSync.rtpInput->input.maximumSenderClockResidualNs->nanoseconds())); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.video_cname_timeout_ns", std::to_string(identityEvidenceTimeoutNs)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.audio_cname_timeout_ns", std::to_string(identityEvidenceTimeoutNs)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set("rtp_clock_group.require_matching_cname",
                          "false"); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    if (auto status = set("rtp_clock_group.maximum_sender_clock_rate_error_ppm", std::to_string(*avSync.rtpInput->input.maximumSenderClockRateErrorPpm)); !status) return ::media::Result<MediaNodeId>::failure(status.error());
    if (auto status = set(
            "rtp_clock_group.common_epoch_policy",
            mediaRtpCommonEpochPolicyOptionValue(
                avSync.rtpInput->input.commonEpochPolicy)); !status) {
        return ::media::Result<MediaNodeId>::failure(status.error());
    }
    for (MediaNodeId input : {videoInput, audioInput}) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(
                graph, owner, input, "rtcp.maximum_extrapolation_ns",
                std::to_string(avSync.rtpInput->input.maximumExtrapolationNs->nanoseconds())); !status) {
            return ::media::Result<MediaNodeId>::failure(status.error());
        }
    }

    const struct PortSpec {
        const char* name;
    } inputs[] = {{"video_clock"}, {"video_event"}, {"audio_clock"}, {"audio_event"}};
    for (const auto& input : inputs) {
        if (auto status = MediaGraphBuildSupport::addInputPortChecked(
                graph, owner, group, input.name, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false); !status) {
            return ::media::Result<MediaNodeId>::failure(status.error());
        }
    }
    if (auto status = MediaGraphBuildSupport::addOutputPortChecked(
            graph, owner, group, "clock_group", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, false, true); !status) {
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
    if (auto status = MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
            plan); !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    MediaGraph graph;

    const MediaNodeKind inputKind = plan.inputType == RealtimeInputType::RtpPort
        ? MediaNodeKind::RawRtpInput
        : MediaNodeKind::RealtimeInput;
    const bool includeAudio = branchEnabled(plan.audioPlan);
    const bool synchronized = plan.avSyncRuntime.has_value();
    const MediaRealtimeSingleStreamOutputPlan* singleStreamOutput =
        plan.singleStreamOutput ? &*plan.singleStreamOutput : nullptr;
    if (includeAudio && !synchronized) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime A/V graph construction requires the planned synchronization runtime"));
    }
    const std::optional<PacketSelectOutputPlan> videoPacketOutput{
        packetOutputPlan(plan.videoPlan.sourceStreamIndex,
                         plan.videoPlan.branchMode,
                         singleStreamOutput
                             ? singleStreamOutput->packetCopyNormalizationRequired
                             : false,
                         synchronized)};
    const std::optional<PacketSelectOutputPlan> audioPacketOutput = includeAudio
        ? std::optional<PacketSelectOutputPlan>{packetOutputPlan(
              plan.audioPlan.sourceStreamIndex, plan.audioPlan.branchMode,
              false, synchronized)}
        : std::nullopt;
    MediaNodeId videoMux = MediaNodeId::invalid();

    if (singleStreamOutput && separateStreamsOutput(plan)) {
        const MediaNodeId videoOutput = graph.addNode(MediaNodeKind::RtpOutput,
                                                      "realtime.video.rtp.output",
                                                      "Realtime video RTP output context");
        videoMux = graph.addNode(MediaNodeKind::RtpMux,
                                 "realtime.video.rtp.mux",
                                 "Realtime video RTP mux");
        const MediaNodeId sdp = graph.addNode(MediaNodeKind::SdpWriter,
                                              "realtime.sdp.writer",
                                              "Realtime SDP writer");

        if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, videoOutput, singleStreamOutput->rtpOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaRealtimeOptionApplier::applyOutputOptions(graph, videoMux, singleStreamOutput->rtpOutput); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaRealtimeOptionApplier::applySdpWriterOptions(graph, sdp, singleStreamOutput->sdp); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = MediaRealtimeOptionApplier::applyMuxOptions(graph, videoMux, singleStreamOutput->mux); !status) return ::media::Result<MediaGraph>::failure(status.error());

        if (auto status = MediaGraphBuildSupport::addInputPortChecked(graph, owner, sdp, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext, true, true); !status) return ::media::Result<MediaGraph>::failure(status.error());
        if (auto status = addRtpOutputChain(graph, videoOutput, videoMux, sdp, MediaStreamKind::Video, plan.edgePolicies); !status) return ::media::Result<MediaGraph>::failure(status.error());
    } else if (singleStreamOutput) {
        FileOutputSegmentOptions outputOptions;
        outputOptions.prefix = "realtime.mpegts";
        outputOptions.outputUrl = singleStreamOutput->muxedOutput.url;
        outputOptions.outputFormat = singleStreamOutput->muxedOutput.format;
        outputOptions.outputResourceKind =
            singleStreamOutput->muxedOutput.outputResourceKind;
        outputOptions.expectVideo = singleStreamOutput->mux.expectVideo;
        outputOptions.expectAudio = singleStreamOutput->mux.expectAudio;
        if (!singleStreamOutput->muxedOutput.muxSessionKind) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "realtime muxed output requires planner-selected mux session kind"));
        }
        outputOptions.muxSessionKind =
            singleStreamOutput->muxedOutput.muxSessionKind;
        outputOptions.queues = plan.queues;
        auto output = MediaOutputSegmentBuilder::buildFileMuxOutput(graph, outputOptions);
        if (!output) {
            return ::media::Result<MediaGraph>::failure(output.error());
        }
        videoMux = output.value().mux;
    }

    const bool isolateRawRtpAudio = plan.useIsolatedAudioInput;
    auto videoInputChain = addRealtimePacketInputChain(graph,
                                                       inputKind,
                                                       isolateRawRtpAudio ? "realtime.video.input" : "realtime.input",
                                                       "Realtime media input",
                                                       synchronized,
                                                       videoPacketOutput,
                                                       isolateRawRtpAudio
                                                           ? std::optional<PacketSelectOutputPlan>{}
                                                           : audioPacketOutput,
                                                       plan.input,
                                                       plan.queues,
                                                       plan.edgePolicies);
    if (!videoInputChain) {
        return ::media::Result<MediaGraph>::failure(videoInputChain.error());
    }

    RealtimePacketInputChain audioInputChain;
    MediaNodeId protocolClockNode = MediaNodeId::invalid();
    if (isolateRawRtpAudio) {
        auto audioInput = addRealtimePacketInputChain(graph,
                                                      MediaNodeKind::RawRtpInput,
                                                      "realtime.audio.input",
                                                      "Realtime audio RTP input",
                                                      synchronized,
                                                      std::nullopt,
                                                      audioPacketOutput,
                                                      plan.audioInput,
                                                      plan.queues,
                                                      plan.edgePolicies);
        if (!audioInput) {
            return ::media::Result<MediaGraph>::failure(audioInput.error());
        }
        audioInputChain = audioInput.value();

        if (!plan.avSyncRuntime || !plan.input.rtpTransport || !plan.audioInput.rtpTransport) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated RTP A/V inputs require A/V sync and transport plans"));
        }
        auto clockGroup = addRtpClockGroup(graph,
                                           videoInputChain.value().input,
                                           audioInputChain.input,
                                           plan.avSyncRuntime->synchronization,
                                           plan.edgePolicies);
        if (!clockGroup) return ::media::Result<MediaGraph>::failure(clockGroup.error());
        protocolClockNode = clockGroup.value();
    }

    MediaNodeId videoPacketSourceNode = videoInputChain.value().packetSelect.split;
    std::string videoPacketSourcePort = plan.inputType == RealtimeInputType::RtpPort ? "packet" : "video";
    MediaNodeId audioPacketSourceNode = isolateRawRtpAudio
        ? audioInputChain.packetSelect.split
        : videoInputChain.value().packetSelect.split;
    std::string audioPacketSourcePort = isolateRawRtpAudio ? "packet" : "audio";
    std::optional<MediaRealtimeAvSyncInputEndpoints> synchronizedInput;
    if (plan.avSyncRuntime) {
        if (!protocolClockNode.isValid()) {
            if (!videoInputChain.value().packetSelect.demux.isValid()) {
                return ::media::Result<MediaGraph>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized MPEG-TS input requires a selected-program demux clock"));
            }
            protocolClockNode = videoInputChain.value().packetSelect.demux;
        }
        MediaRealtimeAvSyncInputSegmentOptions syncOptions;
        syncOptions.prefix = "realtime.av_sync";
        syncOptions.sources.videoPacket =
            MediaEndpoint{videoPacketSourceNode, videoPacketSourcePort};
        syncOptions.sources.audioPacket =
            MediaEndpoint{audioPacketSourceNode, audioPacketSourcePort};
        syncOptions.sources.protocolClock = MediaEndpoint{
            protocolClockNode,
            isolateRawRtpAudio ? "clock_group" : "clock"};
        syncOptions.releasedVideoStreamIndex = plan.videoPlan.sourceStreamIndex;
        syncOptions.releasedAudioStreamIndex = plan.audioPlan.sourceStreamIndex;
        syncOptions.releasedVideoEdgeKind =
            plan.videoPlan.branchMode == MediaBranchMode::CopyPacket
                ? MediaEdgeKind::EncodedPacket
                : MediaEdgeKind::InputPacket;
        syncOptions.releasedAudioEdgeKind = MediaEdgeKind::InputPacket;
        auto assembled = MediaRealtimeAvSyncInputSegmentBuilder::build(
            graph, syncOptions, *plan.avSyncRuntime);
        if (!assembled) {
            return ::media::Result<MediaGraph>::failure(assembled.error());
        }
        synchronizedInput = std::move(assembled).value();
        videoPacketSourceNode = synchronizedInput->releasedVideo.node;
        videoPacketSourcePort = synchronizedInput->releasedVideo.port;
        audioPacketSourceNode = synchronizedInput->releasedAudio.node;
        audioPacketSourcePort = synchronizedInput->releasedAudio.port;
    }

    MediaVideoBranchSegmentOptions videoOptions;
    videoOptions.prefix = "realtime.video";
    videoOptions.plan = std::move(plan.videoPlan);
    videoOptions.parameters = plan.videoParameters;
    videoOptions.queues = plan.queues;
    videoOptions.edgePolicies = plan.edgePolicies;
    if (plan.avSyncRuntime) {
        videoOptions.edgePolicies.videoPacket =
            plan.edgePolicies.synchronizedPacket;
    }
    videoOptions.inputStartRequiresKeyFrame = synchronizedInput
        ? false : plan.videoInputStartRequiresKeyFrame;
    if (plan.avSyncRuntime) {
        if (plan.avSyncRuntime->queues.frame == 0) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized realtime video requires positive planned lineage capacity"));
        }
        videoOptions.canonicalLineageCapacity =
            plan.avSyncRuntime->queues.frame;
    }
    videoOptions.formatSourceNode = videoInputChain.value().input;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = videoPacketSourceNode;
    videoOptions.packetSourcePort = videoPacketSourcePort;
    videoOptions.normalizePacketCopy = singleStreamOutput
        ? singleStreamOutput->packetCopyNormalizationRequired
        : false;
    auto video = MediaVideoBranchSegmentBuilder::build(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }

    std::optional<MediaEncodedBranchEndpoints> audio;
    if (includeAudio) {
        const auto& avSyncRuntime = *plan.avSyncRuntime;
        MediaAudioBranchSegmentOptions audioOptions;
        audioOptions.prefix = "realtime.audio";
        audioOptions.plan = std::move(plan.audioPlan);
        audioOptions.queues = plan.queues;
        audioOptions.edgePolicies = plan.edgePolicies;
        audioOptions.edgePolicies.audioPacket =
            plan.edgePolicies.synchronizedPacket;
        audioOptions.formatSourceNode = isolateRawRtpAudio
            ? audioInputChain.input
            : videoInputChain.value().input;
        audioOptions.formatSourcePort = "format";
        audioOptions.packetSourceNode = audioPacketSourceNode;
        audioOptions.packetSourcePort = audioPacketSourcePort;
        audioOptions.normalizeInputPackets = false;
        audioOptions.correctionMode =
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
        audioOptions.lineageMode =
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
        audioOptions.lineageCapacity = avSyncRuntime.queues.frame;
        audioOptions.correctionGeneration = MediaFirstLockedSourceGeneration;
        audioOptions.correctionLookaheadWindows =
            avSyncRuntime.synchronization.audioServo.correctionLookaheadWindows;
        audioOptions.syncGroup = avSyncRuntime.groupKey;
        auto builtAudio = MediaAudioBranchSegmentBuilder::build(
            graph, audioOptions);
        if (!builtAudio) {
            return ::media::Result<MediaGraph>::failure(builtAudio.error());
        }
        audio = std::move(builtAudio).value();
    }

    if (plan.avSyncRuntime) {
        if (!synchronizedInput || !audio) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized realtime output requires complete A/V endpoints"));
        }
        MediaRealtimeAvSchedulerSegmentOptions schedulerOptions;
        schedulerOptions.prefix = "realtime.av_sync.output";
        schedulerOptions.canonicalVideo = video.value().packet;
        schedulerOptions.canonicalAudio = audio->packet;
        auto scheduled = MediaRealtimeAvSchedulerSegmentBuilder::build(
            graph, schedulerOptions, *plan.avSyncRuntime);
        if (!scheduled) {
            return ::media::Result<MediaGraph>::failure(scheduled.error());
        }
        if (plan.avSyncRuntime->outputAdapter ==
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
            MediaScheduledRtpOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.rtp_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduledVideo = scheduled.value().video;
            outputOptions.scheduledAudio = scheduled.value().audio;
            auto output = MediaScheduledRtpOutputSegmentBuilder::build(
                graph, outputOptions, *plan.avSyncRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else if (plan.avSyncRuntime->outputAdapter ==
                   MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
            MediaScheduledMpegTsOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.mpegts_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduled = scheduled.value().serialized;
            auto output = MediaScheduledMpegTsOutputSegmentBuilder::build(
                graph, outputOptions, *plan.avSyncRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "Synchronized realtime output requires a production adapter"));
        }
    } else {
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, video.value().codec.node,
                video.value().codec.port, videoMux, "codec",
                "realtime.video.codec -> output.codec",
                plan.edgePolicies.metadata); !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
        if (auto status = MediaGraphBuildSupport::connectChecked(
                graph, owner, video.value().packet.node,
                video.value().packet.port, videoMux, "packet",
                "realtime.video.packet -> output.packet",
                plan.edgePolicies.videoMux); !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
    }

    return ::media::Result<MediaGraph>::success(std::move(graph));
}

::media::Result<MediaRealtimeExecutableGraph> MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
    MediaRealtimeTranscodePreflight preflight)
{
    if (auto status = MediaRealtimeTsInputPlanValidator::validate(
            preflight.plan.inputType, preflight.plan.input); !status) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(status.error());
    }
    const RealtimeInputType inputType = preflight.plan.inputType;
    const bool requiresPrepared = inputType != RealtimeInputType::RtpPort;
    if (requiresPrepared && (!preflight.prepared || !preflight.prepared->valid())) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized("realtime executable graph requires prepared input"));
    }
    std::optional<MediaAvSyncRuntimeBinding> avSyncBinding;
    if (preflight.plan.avSyncRuntime) {
        const auto& runtimePlan = *preflight.plan.avSyncRuntime;
        avSyncBinding = MediaAvSyncRuntimeBinding{
            runtimePlan.groupKey,
            runtimePlan.synchronization,
            runtimePlan.transition,
            MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput,
            runtimePlan.outputAdapter};
    }
    auto graphResult = build(std::move(preflight.plan));
    if (!graphResult) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(graphResult.error());
    }
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graphResult).value();
    executable.avSyncBinding = std::move(avSyncBinding);
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
            MediaPreparedRealtimeInputBinding{
                inputId,
                inputType == RealtimeInputType::MpegTsUdp
                    ? MediaPreparedRealtimeInputKind::MpegTs
                    : MediaPreparedRealtimeInputKind::Generic,
                std::move(*preflight.prepared)});
    }
    return ::media::Result<MediaRealtimeExecutableGraph>::success(std::move(executable));
}

} // namespace media::ffmpeg::graph
