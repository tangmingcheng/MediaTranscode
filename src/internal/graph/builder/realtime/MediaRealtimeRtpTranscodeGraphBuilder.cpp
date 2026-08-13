#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeTsInputPlanValidator.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeVideoSchedulerSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <string>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeRtpTranscodeGraphBuilder";

struct RealtimePacketInputChain {
    MediaNodeId input;
    PacketSelectSegment packetSelect;
};

::media::Result<MediaNodeId> findPreparedInputTarget(
    const MediaGraph& graph,
    MediaPreparedRealtimeInputKind expectedKind,
    std::optional<std::string_view> rawRtpStream)
{
    if ((expectedKind == MediaPreparedRealtimeInputKind::RawRtp) !=
        rawRtpStream.has_value()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared input target requires an exact kind and stream identity"));
    }
    MediaNodeId target = MediaNodeId::invalid();
    for (const MediaNode& node : graph.nodes()) {
        const bool matches = rawRtpStream
            ? node.kind == MediaNodeKind::RawRtpInput &&
                node.options.has("rtp.stream_kind") &&
                node.options.value("rtp.stream_kind") == *rawRtpStream
            : node.kind == MediaNodeKind::RealtimeInput;
        if (!matches) continue;
        if (target.isValid()) {
            return ::media::Result<MediaNodeId>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "realtime executable graph has duplicate prepared input targets"));
        }
        target = node.id;
    }
    if (!target.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph has no prepared input target"));
    }
    return ::media::Result<MediaNodeId>::success(target);
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

::media::Result<RealtimePacketInputChain> addRealtimePacketInputChain(
    MediaGraph& graph,
    MediaNodeKind inputKind,
    const std::string& prefix,
    const std::string& label,
    bool requiresProtocolClock,
    const std::optional<PacketSelectOutputPlan>& videoOutput,
    const std::optional<PacketSelectOutputPlan>& audioOutput,
    const MediaRealtimeRtpInputNodePlan& inputPlan,
    const MediaRealtimeEdgePolicySet& edgePolicies,
    const MediaEdgePolicy& genericPacketPolicy)
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
    packetSelectOptions.metadataPolicy = edgePolicies.metadata;
    packetSelectOptions.packetPolicy = genericPacketPolicy;
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

bool branchEnabled(const MediaAudioPipelinePlan& plan) noexcept
{
    return plan.enabled && plan.branchMode != MediaBranchMode::Drop;
}

bool branchEnabled(const MediaPipelinePlan& plan) noexcept
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
    return buildPlanned(plan);
}

::media::Result<MediaGraph> MediaRealtimeRtpTranscodeGraphBuilder::buildPlanned(
    MediaRealtimeRtpTranscodePlan& plan)
{
    if (auto status = MediaRealtimeRtpTranscodePlanner::validatePlannedProduct(
            plan); !status) {
        return ::media::Result<MediaGraph>::failure(status.error());
    }
    MediaGraph graph;

    const MediaNodeKind inputKind = plan.inputType == RealtimeInputType::RtpPort
        ? MediaNodeKind::RawRtpInput
        : MediaNodeKind::RealtimeInput;
    const auto* videoRuntime =
        std::get_if<MediaRealtimeVideoRuntimePlan>(&plan.runtime);
    const auto* avRuntime =
        std::get_if<MediaRealtimeAvSyncRuntimePlan>(&plan.runtime);
    if ((videoRuntime == nullptr) == (avRuntime == nullptr)) {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime graph requires exactly one typed runtime product"));
    }
    const MediaGraphQueueParameters& queues = videoRuntime
        ? videoRuntime->queues
        : avRuntime->queues;
    const MediaRealtimeEdgePolicySet& edgePolicies = videoRuntime
        ? videoRuntime->edgePolicies
        : avRuntime->edgePolicies;
    const bool synchronized = avRuntime != nullptr;
    const bool audioBranchEnabled = synchronized;
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput =
        avRuntime && avRuntime->isolatedAudioInput
        ? &*avRuntime->isolatedAudioInput
        : nullptr;
    const bool videoPacketNormalization = videoRuntime
        ? videoRuntime->packetCopyNormalizationRequired
        : false;
    const std::optional<PacketSelectOutputPlan> videoPacketOutput{
        packetOutputPlan(plan.videoPlan.sourceStreamIndex,
                         plan.videoPlan.branchMode,
                         videoPacketNormalization,
                         synchronized)};
    const std::optional<PacketSelectOutputPlan> audioPacketOutput = audioBranchEnabled
        ? std::optional<PacketSelectOutputPlan>{packetOutputPlan(
              avRuntime->audioPipeline.sourceStreamIndex,
              avRuntime->audioPipeline.branchMode,
              false, synchronized)}
        : std::nullopt;
    const bool isolateRawRtpAudio = isolatedAudioInput != nullptr;
    const MediaEdgePolicy& videoIngressPacketPolicy = videoRuntime
        ? videoRuntime->lineageEdgePolicies.ingressPacket
        : edgePolicies.synchronizedPacket;
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
                                                       edgePolicies,
                                                       videoIngressPacketPolicy);
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
                                                      *isolatedAudioInput,
                                                      edgePolicies,
                                                      edgePolicies.synchronizedPacket);
        if (!audioInput) {
            return ::media::Result<MediaGraph>::failure(audioInput.error());
        }
        audioInputChain = audioInput.value();

        if (!avRuntime || !plan.input.rtpTransport ||
            !isolatedAudioInput->rtpTransport) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated RTP A/V inputs require A/V sync and transport plans"));
        }
        auto clockGroup = addRtpClockGroup(graph,
                                           videoInputChain.value().input,
                                           audioInputChain.input,
                                           avRuntime->synchronization,
                                           edgePolicies);
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
    if (avRuntime) {
        if (!avRuntime->synchronization.sourceClockMode) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized realtime input requires its planned source clock mode"));
        }
        switch (*avRuntime->synchronization.sourceClockMode) {
        case MediaAvSyncSourceClockMode::RtpSenderReports:
            if (!protocolClockNode.isValid()) {
                return ::media::Result<MediaGraph>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized RTP input requires its planned RTP clock group"));
            }
            break;
        case MediaAvSyncSourceClockMode::MpegTsPcr:
            if (!videoInputChain.value().packetSelect.demux.isValid()) {
                return ::media::Result<MediaGraph>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized MPEG-TS input requires a selected-program demux clock"));
            }
            protocolClockNode = videoInputChain.value().packetSelect.demux;
            break;
        case MediaAvSyncSourceClockMode::DemuxTimestamps:
            if (protocolClockNode.isValid()) {
                return ::media::Result<MediaGraph>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Demux timestamp input rejects an external protocol clock"));
            }
            break;
        default:
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "Synchronized realtime input source clock mode is unsupported"));
        }
        MediaRealtimeAvSyncInputSegmentOptions syncOptions;
        syncOptions.prefix = "realtime.av_sync";
        syncOptions.sources.videoPacket =
            MediaEndpoint{videoPacketSourceNode, videoPacketSourcePort};
        syncOptions.sources.audioPacket =
            MediaEndpoint{audioPacketSourceNode, audioPacketSourcePort};
        syncOptions.sources.protocolClock = protocolClockNode.isValid()
            ? MediaEndpoint{
                  protocolClockNode,
                  isolateRawRtpAudio ? "clock_group" : "clock"}
            : MediaEndpoint{};
        syncOptions.releasedVideoStreamIndex = plan.videoPlan.sourceStreamIndex;
        syncOptions.releasedAudioStreamIndex =
            avRuntime->audioPipeline.sourceStreamIndex;
        syncOptions.releasedVideoEdgeKind =
            plan.videoPlan.branchMode == MediaBranchMode::CopyPacket
                ? MediaEdgeKind::EncodedPacket
                : MediaEdgeKind::InputPacket;
        syncOptions.releasedAudioEdgeKind =
            avRuntime->audioPipeline.branchMode == MediaBranchMode::CopyPacket
                ? MediaEdgeKind::EncodedPacket
                : MediaEdgeKind::InputPacket;
        auto assembled = MediaRealtimeAvSyncInputSegmentBuilder::build(
            graph, syncOptions, *avRuntime);
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
    videoOptions.queues = queues;
    videoOptions.edgePolicies = edgePolicies;
    if (videoRuntime) {
        videoOptions.lineageEdgePolicies =
            videoRuntime->lineageEdgePolicies;
    }
    if (avRuntime) {
        videoOptions.edgePolicies.videoPacket =
            edgePolicies.atomicVideoPacket;
    }
    videoOptions.inputStartRequiresKeyFrame = synchronizedInput
        ? false : plan.videoInputStartRequiresKeyFrame;
    if (avRuntime) {
        if (!avRuntime->synchronization.startup
                 .requireVideoKeyFrame) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized realtime video requires planner generation-start key-frame policy"));
        }
        if (avRuntime->queues.frame == 0) {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized realtime video requires positive planned lineage capacity"));
        }
        videoOptions.canonicalLineageCapacity =
            avRuntime->queues.frame;
        videoOptions.generationStartRequiresKeyFrame =
            *avRuntime->synchronization.startup
                 .requireVideoKeyFrame;
    }
    videoOptions.formatSourceNode = videoInputChain.value().input;
    videoOptions.formatSourcePort = "format";
    videoOptions.packetSourceNode = videoPacketSourceNode;
    videoOptions.packetSourcePort = videoPacketSourcePort;
    videoOptions.normalizePacketCopy = videoPacketNormalization;
    auto video = MediaVideoBranchSegmentBuilder::build(graph, videoOptions);
    if (!video) {
        return ::media::Result<MediaGraph>::failure(video.error());
    }

    std::optional<MediaEncodedBranchEndpoints> audio;
    if (audioBranchEnabled) {
        const auto& avSyncRuntime = *avRuntime;
        MediaAudioBranchSegmentOptions audioOptions;
        audioOptions.prefix = "realtime.audio";
        audioOptions.plan = std::move(avRuntime->audioPipeline);
        audioOptions.queues = queues;
        audioOptions.edgePolicies = edgePolicies;
        audioOptions.edgePolicies.audioPacket =
            edgePolicies.atomicAudioPacket;
        audioOptions.formatSourceNode = isolateRawRtpAudio
            ? audioInputChain.input
            : videoInputChain.value().input;
        audioOptions.formatSourcePort = "format";
        audioOptions.packetSourceNode = audioPacketSourceNode;
        audioOptions.packetSourcePort = audioPacketSourcePort;
        audioOptions.normalizeInputPackets = false;
        if (auto status = mapSynchronizedAudioBranchOptions(
                avSyncRuntime, audioOptions); !status) {
            return ::media::Result<MediaGraph>::failure(status.error());
        }
        auto builtAudio = MediaAudioBranchSegmentBuilder::build(
            graph, audioOptions);
        if (!builtAudio) {
            return ::media::Result<MediaGraph>::failure(builtAudio.error());
        }
        audio = std::move(builtAudio).value();
    }

    if (avRuntime) {
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
            graph, schedulerOptions, *avRuntime);
        if (!scheduled) {
            return ::media::Result<MediaGraph>::failure(scheduled.error());
        }
        if (avRuntime->outputAdapter ==
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
            MediaScheduledRtpOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.rtp_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduledVideo = scheduled.value().video;
            outputOptions.scheduledAudio = scheduled.value().audio;
            auto output = MediaScheduledRtpOutputSegmentBuilder::build(
                graph, outputOptions, *avRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else if (avRuntime->outputAdapter ==
                   MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
            MediaScheduledMpegTsOutputSegmentOptions outputOptions;
            outputOptions.prefix = "realtime.av_sync.mpegts_output";
            outputOptions.epochActivated = synchronizedInput->activatedRelease;
            outputOptions.videoCodec = video.value().codec;
            outputOptions.audioCodec = audio->codec;
            outputOptions.scheduled = scheduled.value().serialized;
            outputOptions.expectVideo =
                branchEnabled(plan.videoPlan);
            outputOptions.expectAudio = true;
            auto output = MediaScheduledMpegTsOutputSegmentBuilder::build(
                graph, outputOptions, *avRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "Synchronized realtime output requires a production adapter"));
        }
    } else if (videoRuntime) {
        MediaRealtimeVideoSchedulerSegmentOptions schedulerOptions;
        schedulerOptions.prefix = "realtime.video_only.output";
        schedulerOptions.encodedVideo = video.value().packet;
        auto scheduled = MediaRealtimeVideoSchedulerSegmentBuilder::build(
            graph, schedulerOptions, *videoRuntime);
        if (!scheduled) {
            return ::media::Result<MediaGraph>::failure(scheduled.error());
        }
        if (std::holds_alternative<
                MediaVideoOnlySeparateRtpOutputRuntimePlan>(
                videoRuntime->outputAdapter)) {
            auto output = MediaScheduledRtpOutputSegmentBuilder::buildVideoOnly(
                graph,
                MediaVideoOnlyScheduledRtpOutputSegmentOptions{
                    "realtime.video_only.rtp_output",
                    scheduled.value().activation,
                    video.value().codec,
                    scheduled.value().scheduledVideo},
                *videoRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else if (std::holds_alternative<
                       MediaProjectMpegTsRuntimeOutputPlan>(
                       videoRuntime->outputAdapter)) {
            auto output =
                MediaScheduledMpegTsOutputSegmentBuilder::buildVideoOnly(
                    graph,
                    MediaVideoOnlyScheduledMpegTsOutputSegmentOptions{
                        "realtime.video_only.mpegts_output",
                        scheduled.value().activation,
                        video.value().codec,
                        scheduled.value().scheduledVideo},
                    *videoRuntime);
            if (!output) {
                return ::media::Result<MediaGraph>::failure(output.error());
            }
        } else {
            return ::media::Result<MediaGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "VideoOnly runtime requires a production output adapter"));
        }
    } else {
        return ::media::Result<MediaGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime graph lost its typed runtime variant"));
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
    const auto requiredPreparedKind =
        preflight.plan.requiredPreparedInputKind;
    const bool requiresPrepared = requiredPreparedKind.has_value();
    if (!requiresPrepared && inputType != RealtimeInputType::RtpPort) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "non-RTP realtime executable graph requires prepared input"));
    }
    if (!requiresPrepared && preflight.prepared) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "node-owned raw RTP plan rejects an unplanned prepared input"));
    }
    if (requiresPrepared && (!preflight.prepared || !preflight.prepared->valid() ||
        preflight.prepared->kind() != requiredPreparedKind)) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph requires the exact planner-selected prepared input"));
    }
    bool requiresPreparedAudio = false;
    const auto* avRuntime = std::get_if<MediaRealtimeAvSyncRuntimePlan>(
        &preflight.plan.runtime);
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput =
        avRuntime && avRuntime->isolatedAudioInput
        ? &*avRuntime->isolatedAudioInput
        : nullptr;
    if (isolatedAudioInput) {
        if (!isolatedAudioInput->requiresPreparedInput) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated realtime audio input requires an explicit prepared-input ownership decision"));
        }
        requiresPreparedAudio =
            *isolatedAudioInput->requiresPreparedInput;
    }
    if (requiresPreparedAudio &&
        (!preflight.preparedAudio || !preflight.preparedAudio->valid() ||
         preflight.preparedAudio->kind() !=
             MediaPreparedRealtimeInputKind::RawRtp)) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime executable graph requires synchronized prepared RTP audio input"));
    }
    if (!requiresPreparedAudio && preflight.preparedAudio) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime executable graph rejects unplanned prepared audio input"));
    }
    auto graphResult = buildPlanned(preflight.plan);
    if (!graphResult) {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            graphResult.error());
    }

    MediaRealtimeRuntimeBinding runtimeBinding;
    if (auto* videoRuntime = std::get_if<MediaRealtimeVideoRuntimePlan>(
            &preflight.plan.runtime)) {
        runtimeBinding.emplace<MediaRealtimeVideoRuntimeBinding>(
            MediaRealtimeVideoRuntimeBinding{
                std::move(*videoRuntime),
                std::move(preflight.plan.input.rtpTransport)});
    } else if (auto* runtimePlan =
                   std::get_if<MediaRealtimeAvSyncRuntimePlan>(
                       &preflight.plan.runtime)) {
        const auto audioExecutionProduct =
            std::holds_alternative<MediaSynchronizedAudioPacketCopyBounds>(
                runtimePlan->componentBounds)
                ? MediaSynchronizedAudioExecutionProduct::PacketCopy
                : MediaSynchronizedAudioExecutionProduct::FrameTranscode;
        auto outputProduct = std::visit(
            []<typename Product>(Product&& product)
                -> MediaAvSyncRuntimeOutputProduct {
                return MediaAvSyncRuntimeOutputProduct(
                    std::forward<Product>(product));
            },
            std::move(runtimePlan->protocolOutput));
        runtimeBinding.emplace<MediaAvSyncRuntimeBinding>(
            MediaAvSyncRuntimeBinding{
            std::move(runtimePlan->groupKey),
            std::move(runtimePlan->synchronization),
            std::move(runtimePlan->transition),
            runtimePlan->edgePolicies,
            audioExecutionProduct,
            std::move(outputProduct)});
    } else {
        return ::media::Result<MediaRealtimeExecutableGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime executable graph requires a typed runtime binding"));
    }
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graphResult).value();
    executable.runtimeBinding = std::move(runtimeBinding);
    if (requiresPrepared) {
        auto inputId = findPreparedInputTarget(
            executable.graph, *requiredPreparedKind,
            *requiredPreparedKind == MediaPreparedRealtimeInputKind::RawRtp
                ? std::optional<std::string_view>("video")
                : std::nullopt);
        if (!inputId) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                inputId.error());
        }
        executable.inputBindings.push_back(
            MediaPreparedRealtimeInputBinding{
                inputId.value(),
                *requiredPreparedKind,
                std::move(*preflight.prepared)});
    }
    if (requiresPreparedAudio) {
        auto audioInputId = findPreparedInputTarget(
            executable.graph, MediaPreparedRealtimeInputKind::RawRtp,
            std::optional<std::string_view>("audio"));
        if (!audioInputId) {
            return ::media::Result<MediaRealtimeExecutableGraph>::failure(
                audioInputId.error());
        }
        executable.inputBindings.push_back(
            MediaPreparedRealtimeInputBinding{
                audioInputId.value(),
                MediaPreparedRealtimeInputKind::RawRtp,
                std::move(*preflight.preparedAudio)});
    }
    return ::media::Result<MediaRealtimeExecutableGraph>::success(std::move(executable));
}

} // namespace media::ffmpeg::graph
