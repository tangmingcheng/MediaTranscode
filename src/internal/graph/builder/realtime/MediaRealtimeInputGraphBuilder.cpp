#include "internal/graph/builder/realtime/MediaRealtimeInputGraphBuilder.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"
#include "internal/graph/builder/segments/MediaPacketSelectSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"
#include <tuple>

namespace media::ffmpeg::graph {
namespace {
constexpr const char* owner = "MediaRealtimeInputGraphBuilder";

struct RealtimePacketInputChain {
    MediaNodeId input;
    PacketSelectSegment packetSelect;
    std::vector<MediaNodeId> sourceMembers;
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
        chain.sourceMembers = {input};
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
        chain.sourceMembers = {input, demux};
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
    chain.sourceMembers = {input, chain.packetSelect.demux, chain.packetSelect.split};
    return ::media::Result<RealtimePacketInputChain>::success(chain);
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
    const std::string& prefix,
    MediaNodeId videoInput,
    MediaNodeId audioInput,
    const MediaAvSyncPlan& avSync,
    const MediaRealtimeEdgePolicySet& edgePolicies)
{
    if (!avSync.rtpInput || !avSync.rtpInput->input.clockLossPolicy ||
        !avSync.rtpInput->videoInput.clockRate ||
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
                                            prefix + ".rtp.clock_group",
                                            "Realtime RTP source clock group");
    if (!group.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError("failed to add RTP clock group node"));
    }
    const auto set = [&](const char* key, std::string value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, group, key, value);
    };
    if (auto status = set("rtp_clock_group.invalidate_on_degraded",
            *avSync.rtpInput->input.clockLossPolicy == MediaRtpClockLossPolicy::InvalidateAndWait
                ? "true" : "false"); !status)
        return ::media::Result<MediaNodeId>::failure(status.error());
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

::media::Result<MediaRealtimeInputGraph> MediaRealtimeInputGraphBuilder::append(
    MediaGraph& graph, const std::string& prefix,
    const MediaRealtimeRtpTranscodePlan& plan)
{
    if (prefix.empty()) return ::media::Result<MediaRealtimeInputGraph>::failure(
        ::media::ErrorInfo::invalidArgument("Realtime input requires a graph prefix"));
    const MediaNodeKind inputKind = plan.inputType == RealtimeInputType::RtpPort
        ? MediaNodeKind::RawRtpInput
        : MediaNodeKind::RealtimeInput;
    const auto* videoRuntime =
        std::get_if<MediaRealtimeVideoRuntimePlan>(&plan.runtime);
    const auto* avRuntime =
        std::get_if<MediaRealtimeAvSyncRuntimePlan>(&plan.runtime);
    if ((videoRuntime == nullptr) == (avRuntime == nullptr)) {
        return ::media::Result<MediaRealtimeInputGraph>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime graph requires exactly one typed runtime product"));
    }
    const MediaRealtimeEdgePolicySet& edgePolicies = videoRuntime
        ? videoRuntime->edgePolicies
        : avRuntime->edgePolicies;
    std::optional<MediaAvRuntimeRegistrationPlan> registration;
    if (avRuntime) registration.emplace();
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
                                                       isolateRawRtpAudio ? prefix + ".video.input" : prefix + ".input",
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
        return ::media::Result<MediaRealtimeInputGraph>::failure(videoInputChain.error());
    }

    if (registration) registration->processing.source = videoInputChain.value().sourceMembers;

    RealtimePacketInputChain audioInputChain;
    MediaNodeId protocolClockNode = MediaNodeId::invalid();
    if (isolateRawRtpAudio) {
        auto audioInput = addRealtimePacketInputChain(graph,
                                                      MediaNodeKind::RawRtpInput,
                                                      prefix + ".audio.input",
                                                      "Realtime audio RTP input",
                                                      synchronized,
                                                      std::nullopt,
                                                      audioPacketOutput,
                                                      *isolatedAudioInput,
                                                      edgePolicies,
                                                      edgePolicies.synchronizedPacket);
        if (!audioInput) {
            return ::media::Result<MediaRealtimeInputGraph>::failure(audioInput.error());
        }
        audioInputChain = audioInput.value();
        if (registration) registration->processing.source.insert(
            registration->processing.source.end(), audioInputChain.sourceMembers.begin(),
            audioInputChain.sourceMembers.end());

        if (!avRuntime || !plan.input.rtpTransport ||
            !isolatedAudioInput->rtpTransport) {
            return ::media::Result<MediaRealtimeInputGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "isolated RTP A/V inputs require A/V sync and transport plans"));
        }
        auto clockGroup = addRtpClockGroup(graph, prefix,
                                           videoInputChain.value().input,
                                           audioInputChain.input,
                                           avRuntime->synchronization,
                                           edgePolicies);
        if (!clockGroup) return ::media::Result<MediaRealtimeInputGraph>::failure(clockGroup.error());
        protocolClockNode = clockGroup.value();
        registration->processing.source.push_back(clockGroup.value());
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
            return ::media::Result<MediaRealtimeInputGraph>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized realtime input requires its planned source clock mode"));
        }
        switch (*avRuntime->synchronization.sourceClockMode) {
        case MediaAvSyncSourceClockMode::RtpSenderReports:
            if (!protocolClockNode.isValid()) {
                return ::media::Result<MediaRealtimeInputGraph>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized RTP input requires its planned RTP clock group"));
            }
            break;
        case MediaAvSyncSourceClockMode::MpegTsPcr:
            if (!videoInputChain.value().packetSelect.demux.isValid()) {
                return ::media::Result<MediaRealtimeInputGraph>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized MPEG-TS input requires a selected-program demux clock"));
            }
            protocolClockNode = videoInputChain.value().packetSelect.demux;
            break;
        case MediaAvSyncSourceClockMode::DemuxTimestamps:
            if (protocolClockNode.isValid()) {
                return ::media::Result<MediaRealtimeInputGraph>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Demux timestamp input rejects an external protocol clock"));
            }
            break;
        default:
            return ::media::Result<MediaRealtimeInputGraph>::failure(
                ::media::ErrorInfo::unsupported(
                    "Synchronized realtime input source clock mode is unsupported"));
        }
        MediaRealtimeAvSyncInputSegmentOptions syncOptions;
        syncOptions.prefix = prefix + ".av_sync";
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
            return ::media::Result<MediaRealtimeInputGraph>::failure(assembled.error());
        }
        synchronizedInput = std::move(assembled).value();
        registration->input = synchronizedInput->registration;
        registration->processing.source.insert(registration->processing.source.end(),
            synchronizedInput->sourceMembers.begin(), synchronizedInput->sourceMembers.end());
        videoPacketSourceNode = synchronizedInput->releasedVideo.node;
        videoPacketSourcePort = synchronizedInput->releasedVideo.port;
        audioPacketSourceNode = synchronizedInput->releasedAudio.node;
        audioPacketSourcePort = synchronizedInput->releasedAudio.port;
    }

    MediaRealtimeInputGraph result;
    result.videoFormat = {videoInputChain.value().input, "format"};
    result.audioFormat = {isolateRawRtpAudio ? audioInputChain.input : videoInputChain.value().input, "format"};
    result.videoPacket = {videoPacketSourceNode, videoPacketSourcePort};
    result.audioPacket = {audioPacketSourceNode, audioPacketSourcePort};
    result.synchronized = std::move(synchronizedInput);
    if (registration) result.sourceMembers = std::move(registration->processing.source);
    else result.sourceMembers = std::move(videoInputChain.value().sourceMembers);
    return ::media::Result<MediaRealtimeInputGraph>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
