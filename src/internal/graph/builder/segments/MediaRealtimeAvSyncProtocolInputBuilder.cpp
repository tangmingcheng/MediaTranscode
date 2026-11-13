#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include "internal/graph/model/MediaRtpClockRoute.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.h"

#include "internal/graph/builder/segments/MediaDemuxClockInputSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

using Support = MediaRealtimeAvSyncInputGraphSupport;

::media::Result<void> validateSources(
    const MediaGraph& graph,
    const MediaRealtimeAvSyncInputSources& sources,
    bool requireProtocolClock,
    bool hasAudio)
{
    if (auto status = Support::validateOutput(
            graph, sources.videoPacket, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
        return status;
    }
    if (!hasAudio && sources.audioPacket.valid())
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Video-only protocol input rejects an audio endpoint"));
    if (hasAudio) {
        if (auto status = Support::validateOutput(
                graph, sources.audioPacket, MediaStreamKind::Audio,
                MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
            return status;
        }
    }
    return requireProtocolClock
        ? Support::validateOutput(
              graph, sources.protocolClock, MediaStreamKind::Metadata,
              MediaEdgeKind::Event, MediaPayloadKind::GraphEvent)
        : ::media::Result<void>::success();
}

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> buildRtp(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    const MediaRtpClockRouteShape declaration{*plan.synchronization.members, plan.rtpControlDelivery};
    const bool hasAudio = declaration.has(MediaRtpClockRouteRole::AudioInput);
    std::vector<MediaNodeId> rawInputs{options.sources.videoPacket.node};
    if (hasAudio) rawInputs.push_back(options.sources.audioPacket.node);
    for (const auto rawId : rawInputs) {
        const auto* raw = graph.findNode(rawId);
        if (!raw || raw->kind != MediaNodeKind::RawRtpInput ||
            raw->options.has("rtp.sync_group")) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP source requires uniquely owned raw ingress nodes"));
        }
        if (auto status = Support::setOption(graph, rawId,
                "rtp.sync_group", plan.groupKey.value()); !status)
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(status.error());
    }
    auto snapshotResult = Support::addNode(
        graph, declaration.nodeKind(MediaRtpClockRouteRole::Fanout),
        options.prefix + ".rtp.clock_snapshot", "RTP clock snapshot fanout");
    auto videoResult = Support::addNode(
        graph, declaration.nodeKind(MediaRtpClockRouteRole::VideoBinder),
        options.prefix + ".video.protocol_binder", "RTP video clock binder");
    std::optional<MediaNodeId> audio;
    if (hasAudio) {
        auto added = Support::addNode(graph, declaration.nodeKind(MediaRtpClockRouteRole::AudioBinder),
            options.prefix + ".audio.protocol_binder", "RTP audio clock binder");
        if (!added) return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(added.error());
        audio = added.value();
    }
    auto adapterResult = Support::addNode(
        graph, declaration.nodeKind(MediaRtpClockRouteRole::StateAdapter),
        options.prefix + ".rtp.source_clock_adapter",
        "RTP source clock state adapter");
    if (!snapshotResult || !videoResult || !adapterResult) {
        const auto& error = !snapshotResult ? snapshotResult.error()
            : !videoResult ? videoResult.error() : adapterResult.error();
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(error);
    }
    const MediaNodeId snapshot = snapshotResult.value();
    const MediaNodeId video = videoResult.value();
    const MediaNodeId adapter = adapterResult.value();
    if (!plan.synchronization.members)
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
            ::media::ErrorInfo::notInitialized("RTP source adapter requires planned members"));
    auto encodedMembers = MediaTranscodeStreamSetCodec::encode(*plan.synchronization.members);
    if (!encodedMembers)
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(encodedMembers.error());
    if (auto status = Support::setOption(graph, adapter, "rtp_source_clock.members",
            std::string(encodedMembers.value())); !status)
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(status.error());

    if (auto status = Support::addInput(
            graph, snapshot, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    for (const char* port : {"video", "audio", "startup"}) {
        if (!hasAudio && std::string_view(port) == "audio") continue;
        if (auto status = Support::addOutput(
                graph, snapshot, port, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
    }
    std::vector<std::pair<MediaNodeId, MediaStreamKind>> binders{{video, MediaStreamKind::Video}};
    if (audio) binders.emplace_back(*audio, MediaStreamKind::Audio);
    for (const auto [node, stream] : binders) {
        if (auto status = Support::addInput(
                graph, node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
        if (auto status = Support::addInput(
                graph, node, "clock", MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
        if (auto status = Support::addOutput(
                graph, node, "packet", stream, MediaEdgeKind::InputPacket,
                MediaPayloadKind::Packet); !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
        if (auto status = MediaRealtimeAvSyncNodeConfigurator::
                configureRtpPacketClockBinder(graph, node, stream, plan);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
    }
    if (auto status = Support::addInput(
            graph, adapter, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::addOutput(
            graph, adapter, "state", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }

    std::optional<MediaNodeId> videoControl, audioControl;
    if (plan.rtpControlDelivery == MediaRtpControlDelivery::SharedPreparedSession) {
        const auto addControl = [&](const char* stream) -> ::media::Result<MediaNodeId> {
            auto node = Support::addNode(graph, MediaNodeKind::RtpControlOutput,
                options.prefix + "." + stream + ".rtp.control", "RTP shared session control output");
            if (!node) return node;
            for (const char* port : {"clock", "event"}) {
                if (auto status = Support::addOutput(graph, node.value(), port, MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status)
                    return ::media::Result<MediaNodeId>::failure(status.error());
            }
            if (auto status = Support::setOption(graph, node.value(), "rtp.sync_group", plan.groupKey.value()); !status)
                return ::media::Result<MediaNodeId>::failure(status.error());
            if (auto status = Support::setOption(graph, node.value(), "rtp.control_stream", stream); !status)
                return ::media::Result<MediaNodeId>::failure(status.error());
            return node;
        };
        auto node = addControl("video");
        if (!node) return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(node.error());
        videoControl = node.value();
        if (hasAudio) {
            auto audioNode = addControl("audio");
            if (!audioNode) return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(audioNode.error());
            audioControl = audioNode.value();
        }
    }
    const auto& metadata = plan.edgePolicies.metadata;
    const MediaRtpProtocolNodeBindings bindings{
        {options.sources.protocolClock.node, options.sources.videoPacket.node, video,
            hasAudio ? std::optional{options.sources.audioPacket.node} : std::nullopt, audio, videoControl, audioControl, plan.rtpControlDelivery},
        snapshot, adapter};
    for (const auto& connection : MediaRtpClockRouteConnections) {
        const auto sourceRole = declaration.controlSource(connection.from);
        if (!declaration.has(sourceRole) || !declaration.has(connection.to)) continue;
        const auto from = sourceRole == MediaRtpClockRouteRole::Group
            ? options.sources.protocolClock : MediaEndpoint{bindings.routeNode(sourceRole), connection.output};
        if (auto status = Support::connect(graph, from, bindings.routeNode(connection.to), connection.input,
                connection.label, metadata); !status)
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(status.error());
    }
    if (!plan.synchronization.members || !plan.synchronization.startup.videoReadiness)
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
            ::media::ErrorInfo::notInitialized("RTP source connection facts are absent"));
    auto packets = MediaSourceInputConnectionsPlanner::plan(*plan.synchronization.members,
        *plan.synchronization.startup.videoReadiness, MediaAvSyncSourceClockMode::RtpSenderReports,
        plan.edgePolicies, *std::pmr::new_delete_resource());
    if (!packets) return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(packets.error());
    for (const auto& connection : packets.value()) {
        if (connection.phase != MediaSourceInputConnectionPhase::ProtocolPackets) continue;
        const bool videoPacket = connection.from == MediaSourceInputRole::RawVideo;
        const auto from = videoPacket ? options.sources.videoPacket : options.sources.audioPacket;
        const auto to = videoPacket ? video : audio.value_or(MediaNodeId::invalid());
        if (auto status = Support::connect(graph, from, to, std::string(connection.input),
                std::string(connection.label), connection.policy); !status)
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(status.error());
    }
    std::vector<MediaNodeId> members{snapshot, video, adapter};
    if (audio) members.push_back(*audio);
    if (videoControl) members.push_back(*videoControl);
    if (audioControl) members.push_back(*audioControl);
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::success(
        MediaRealtimeAvSyncProtocolInputEndpoints{
            MediaEndpoint{video, "packet"}, audio ? MediaEndpoint{*audio, "packet"} : MediaEndpoint{},
            MediaEndpoint{adapter, "state"}, std::nullopt, std::move(members),
            bindings});
}

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> buildMpegTs(
    const MediaRealtimeAvSyncInputSegmentOptions& options)
{
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::success(
        MediaRealtimeAvSyncProtocolInputEndpoints{
            options.sources.videoPacket, options.sources.audioPacket,
            options.sources.protocolClock});
}

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> buildDemux(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSourceRuntimePlan& plan,
    const MediaDemuxTimestampInputClockAssemblyPlan& demuxPlan)
{
    auto built = MediaDemuxClockInputSegmentBuilder::build(
        graph,
        MediaDemuxClockInputSegmentOptions{
            options.prefix + ".demux",
            options.sources.videoPacket,
            options.sources.audioPacket,
            plan.groupKey,
            plan.edgePolicies.synchronizedPacket},
        demuxPlan);
    if (!built) {
        return ::media::Result<
            MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
            built.error());
    }
    return ::media::Result<
        MediaRealtimeAvSyncProtocolInputEndpoints>::success(
        MediaRealtimeAvSyncProtocolInputEndpoints{
            built.value().video,
            built.value().audio,
            built.value().sourceClock,
            MediaAvDemuxClockRegistration{built.value().video.node,
                                         built.value().audio.node},
            {built.value().video.node, built.value().audio.node}});
}

} // namespace

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>
MediaRealtimeAvSyncProtocolInputBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSourceRuntimePlan& plan)
{
    if (!plan.synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(::media::ErrorInfo::notInitialized(
                "Synchronized input requires its planner-selected source clock mode"));
    }
    const MediaAvSyncSourceClockMode sourceClock =
        *plan.synchronization.sourceClockMode;
    const bool demux =
        sourceClock == MediaAvSyncSourceClockMode::DemuxTimestamps;
    const bool hasAudio = plan.synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && plan.synchronization.members != MediaTranscodeStreamSet::VideoOnly) ||
        (!hasAudio && sourceClock != MediaAvSyncSourceClockMode::RtpSenderReports))
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument("Single-member protocol input requires planned RTP video"));
    if (auto status = validateSources(
            graph, options.sources, !demux, hasAudio); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    switch (sourceClock) {
    case MediaAvSyncSourceClockMode::RtpSenderReports:
        if (!std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
                plan.assembly.inputClock)) {
            return ::media::Result<
                MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP source clock mode conflicts with its planner input product"));
        }
        return buildRtp(graph, options, plan);
    case MediaAvSyncSourceClockMode::MpegTsPcr:
        if (!std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
                plan.assembly.inputClock)) {
            return ::media::Result<
                MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS source clock mode conflicts with its planner input product"));
        }
        return buildMpegTs(options);
    case MediaAvSyncSourceClockMode::DemuxTimestamps: {
        const auto* demuxPlan =
            std::get_if<MediaDemuxTimestampInputClockAssemblyPlan>(
                &plan.assembly.inputClock);
        if (!demuxPlan) {
            return ::media::Result<
                MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Demux source clock mode conflicts with its planner input product"));
        }
        return buildDemux(graph, options, plan, *demuxPlan);
    }
    }
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Synchronized input requires a supported protocol clock plan"));
}

} // namespace media::ffmpeg::graph
