#include "internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.h"

#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputGraphSupport.h"
#include "internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h"

#include <string>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

using Support = MediaRealtimeAvSyncInputGraphSupport;

::media::Result<void> validateSources(
    const MediaGraph& graph,
    const MediaRealtimeAvSyncInputSources& sources)
{
    if (auto status = Support::validateOutput(
            graph, sources.videoPacket, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
        return status;
    }
    if (auto status = Support::validateOutput(
            graph, sources.audioPacket, MediaStreamKind::Audio,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
        return status;
    }
    return Support::validateOutput(
        graph, sources.protocolClock, MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
}

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> buildRtp(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    auto snapshotResult = Support::addNode(
        graph, MediaNodeKind::RtpClockSnapshotFanout,
        options.prefix + ".rtp.clock_snapshot", "RTP clock snapshot fanout");
    auto videoResult = Support::addNode(
        graph, MediaNodeKind::RtpPacketClockBinder,
        options.prefix + ".video.protocol_binder", "RTP video clock binder");
    auto audioResult = Support::addNode(
        graph, MediaNodeKind::RtpPacketClockBinder,
        options.prefix + ".audio.protocol_binder", "RTP audio clock binder");
    auto adapterResult = Support::addNode(
        graph, MediaNodeKind::RtpSourceClockStateAdapter,
        options.prefix + ".rtp.source_clock_adapter",
        "RTP source clock state adapter");
    if (!snapshotResult || !videoResult || !audioResult || !adapterResult) {
        const auto& error = !snapshotResult ? snapshotResult.error()
            : !videoResult ? videoResult.error()
            : !audioResult ? audioResult.error() : adapterResult.error();
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(error);
    }
    const MediaNodeId snapshot = snapshotResult.value();
    const MediaNodeId video = videoResult.value();
    const MediaNodeId audio = audioResult.value();
    const MediaNodeId adapter = adapterResult.value();

    if (auto status = Support::addInput(
            graph, snapshot, "clock", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    for (const char* port : {"video", "audio", "startup"}) {
        if (auto status = Support::addOutput(
                graph, snapshot, port, MediaStreamKind::Metadata,
                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
    }
    for (const auto [node, stream] : {
             std::pair{video, MediaStreamKind::Video},
             std::pair{audio, MediaStreamKind::Audio}}) {
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

    const auto& metadata = plan.edgePolicies.metadata;
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    if (auto status = Support::connect(
            graph, options.sources.protocolClock, snapshot, "clock",
            "RTP group clock -> snapshot fanout", metadata); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, snapshot, "video", video, "clock",
            "RTP snapshot -> video binder", metadata); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, snapshot, "audio", audio, "clock",
            "RTP snapshot -> audio binder", metadata); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, snapshot, "startup", adapter, "clock",
            "RTP snapshot -> source clock adapter", metadata); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, options.sources.videoPacket, video, "packet",
            "RTP video packet -> clock binder", packet); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, options.sources.audioPacket, audio, "packet",
            "RTP audio packet -> clock binder", packet); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::success(
        MediaRealtimeAvSyncProtocolInputEndpoints{
            MediaEndpoint{video, "packet"}, MediaEndpoint{audio, "packet"},
            MediaEndpoint{adapter, "state"}});
}

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints> buildMpegTs(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    auto videoResult = Support::addNode(
        graph, MediaNodeKind::LockedPacketGate,
        options.prefix + ".video.protocol_binder",
        "MPEG-TS video locked-packet gate");
    auto audioResult = Support::addNode(
        graph, MediaNodeKind::LockedPacketGate,
        options.prefix + ".audio.protocol_binder",
        "MPEG-TS audio locked-packet gate");
    if (!videoResult || !audioResult) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(!videoResult ? videoResult.error() : audioResult.error());
    }
    const MediaNodeId video = videoResult.value();
    const MediaNodeId audio = audioResult.value();
    for (const auto [node, stream] : {
             std::pair{video, MediaStreamKind::Video},
             std::pair{audio, MediaStreamKind::Audio}}) {
        if (auto status = Support::addInput(
                graph, node, "packet", stream,
                MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
            !status) {
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
                graph, node, "packet", stream,
                MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
        if (auto status = MediaRealtimeAvSyncNodeConfigurator::
                configureLockedPacketGate(graph, node, stream, plan);
            !status) {
            return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
                failure(status.error());
        }
    }
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    if (auto status = Support::connect(
            graph, options.sources.videoPacket, video, "packet",
            "MPEG-TS video -> locked-packet gate", packet); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (auto status = Support::connect(
            graph, options.sources.audioPacket, audio, "packet",
            "MPEG-TS audio -> locked-packet gate", packet); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::success(
        MediaRealtimeAvSyncProtocolInputEndpoints{
            MediaEndpoint{video, "packet"}, MediaEndpoint{audio, "packet"},
            options.sources.protocolClock});
}

} // namespace

::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>
MediaRealtimeAvSyncProtocolInputBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (auto status = validateSources(graph, options.sources); !status) {
        return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::
            failure(status.error());
    }
    if (std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
            plan.assembly.inputClock)) {
        return buildRtp(graph, options, plan);
    }
    if (std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
            plan.assembly.inputClock)) {
        return buildMpegTs(graph, options, plan);
    }
    return ::media::Result<MediaRealtimeAvSyncProtocolInputEndpoints>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Synchronized input requires a supported protocol clock plan"));
}

} // namespace media::ffmpeg::graph
