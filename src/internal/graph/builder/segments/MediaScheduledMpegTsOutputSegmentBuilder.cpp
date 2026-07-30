#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner = "MediaScheduledMpegTsOutputSegmentBuilder";

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

::media::Status requireSource(const MediaGraph& graph,
                              const MediaEndpoint& endpoint,
                              MediaStreamKind stream,
                              MediaEdgeKind edge,
                              MediaPayloadKind payload)
{
    const MediaPort* port = endpoint.valid()
        ? graph.findOutputPort(endpoint.node, endpoint.port)
        : nullptr;
    if (!port || port->streamKind != stream || port->edgeKind != edge ||
        port->payloadKind != payload) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output received an endpoint with the wrong type"));
    }
    return ::media::Status::success();
}

::media::Result<MediaNodeId> addProjectMpegTsRtpMux(
    MediaGraph& graph,
    const MediaScheduledMpegTsOutputSegmentOptions& options)
{
    const MediaNodeId mux = graph.addNode(
        MediaNodeKind::FileMux,
        options.prefix + ".mux",
        "Project MPEG-TS mux");
    if (!mux.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError(
                "Scheduled MPEG-TS RTP output failed to add its mux"));
    }
    auto muxKind =
        mediaMuxSessionKindOptionValue(MediaMuxSessionKind::ProjectMpegTs);
    if (!muxKind) {
        return ::media::Result<MediaNodeId>::failure(muxKind.error());
    }
    for (const auto& [key, value] : {
             std::pair{MediaTranscodeOptionKey::MuxExpectVideo,
                       std::string(boolOption(true))},
             std::pair{MediaTranscodeOptionKey::MuxExpectAudio,
                       std::string(boolOption(true))},
             std::pair{MediaTranscodeOptionKey::MuxSessionKind,
                       std::move(muxKind).value()}}) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, mux, key, value);
        if (!set) {
            return ::media::Result<MediaNodeId>::failure(set.error());
        }
    }
    using MediaGraphBuildSupport::addInputPortChecked;
    if (auto added = addInputPortChecked(
            graph, Owner, mux, "codec", MediaStreamKind::Any,
            MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext,
            true, true); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    if (auto added = addInputPortChecked(
            graph, Owner, mux, "packet", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::TsAccessUnit,
            true, true); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    if (auto added = addInputPortChecked(
            graph, Owner, mux, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan,
            true, false); !added) {
        return ::media::Result<MediaNodeId>::failure(added.error());
    }
    return ::media::Result<MediaNodeId>::success(mux);
}

} // namespace

::media::Result<MediaScheduledMpegTsOutputSegmentResult>
MediaScheduledMpegTsOutputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaScheduledMpegTsOutputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    using Result = ::media::Result<MediaScheduledMpegTsOutputSegmentResult>;
    if (options.prefix.empty() || !plan.groupKey.valid() ||
        plan.outputAdapter != MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
        !std::holds_alternative<MediaProjectMpegTsRuntimeOutputPlan>(
            plan.protocolOutput)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output requires its complete planned adapter"));
    }
    for (const auto& [endpoint, stream, edge, payload] : {
             std::tuple{options.epochActivated, MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent},
             std::tuple{options.videoCodec, MediaStreamKind::Video,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
             std::tuple{options.audioCodec, MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext},
             std::tuple{options.scheduled, MediaStreamKind::Any,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet}}) {
        auto valid = requireSource(graph, endpoint, stream, edge, payload);
        if (!valid) return Result::failure(valid.error());
    }
    const bool duplicate = std::any_of(
        graph.nodes().begin(), graph.nodes().end(), [](const MediaNode& node) {
            return node.kind == MediaNodeKind::ProjectMpegTsPlanSource ||
                   node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter ||
                   node.kind == MediaNodeKind::MpegTsRtpSdpPublisher;
        });
    if (duplicate) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output rejects duplicate output authority"));
    }
    const auto& output =
        std::get<MediaProjectMpegTsRuntimeOutputPlan>(plan.protocolOutput);
    MediaNodeId udpOutput = MediaNodeId::invalid();
    MediaNodeId mux = MediaNodeId::invalid();
    MediaNodeId rtpSdpPublisher = MediaNodeId::invalid();
    if (const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(&output.transport)) {
        if (output.protocol.muxPlan().parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams ||
            udp->resourceKind != MediaOutputResourceKind::ByteSink ||
            udp->muxSessionKind != MediaMuxSessionKind::ProjectMpegTs) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled MPEG-TS UDP output requires its exact planned transport variant"));
        }
        auto base = MediaOutputSegmentBuilder::buildFileMuxOutput(
            graph, FileOutputSegmentOptions{
                options.prefix, udp->url, {}, udp->resourceKind, true, true,
                udp->muxSessionKind, plan.queues});
        if (!base) return Result::failure(base.error());
        udpOutput = base.value().fileOutput;
        mux = base.value().mux;
    } else if (std::holds_alternative<MediaMpegTsRtpOutputPlan>(
                   output.transport)) {
        if (output.protocol.muxPlan().parameters().transportKind !=
                MediaOutputTransportKind::RtpAvp) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled MPEG-TS RTP output requires its exact planned transport variant"));
        }
        auto addedMux = addProjectMpegTsRtpMux(graph, options);
        if (!addedMux) return Result::failure(addedMux.error());
        mux = addedMux.value();
        rtpSdpPublisher = graph.addNode(
            MediaNodeKind::MpegTsRtpSdpPublisher,
            options.prefix + ".rtp.sdp.publisher",
            "Atomic MP2T RTP SDP publisher");
        if (!rtpSdpPublisher.isValid()) {
            return Result::failure(::media::ErrorInfo::internalError(
                "Scheduled MPEG-TS RTP output failed to add its SDP publisher"));
        }
        auto groupSet = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, rtpSdpPublisher,
            "mpegts_rtp_sdp.sync_group", plan.groupKey.value());
        if (!groupSet) return Result::failure(groupSet.error());
        auto planPort = MediaGraphBuildSupport::addInputPortChecked(
            graph, Owner, rtpSdpPublisher, "plan",
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan,
            true, false);
        if (!planPort) return Result::failure(planPort.error());
    } else {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled MPEG-TS output requires one planned transport variant"));
    }
    const MediaNodeId planSource = graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource,
        options.prefix + ".plan.source", "Activated project MPEG-TS plan");
    const MediaNodeId adapter = graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter,
        options.prefix + ".scheduled.adapter", "Scheduled MPEG-TS AU adapter");
    if (!planSource.isValid() || !adapter.isValid()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "Scheduled MPEG-TS output failed to add its nodes"));
    }
    auto encoded = MediaProjectMpegTsPlanSourceNodePlanCodec::apply(
        graph, planSource, plan.groupKey, output);
    if (!encoded) return Result::failure(encoded.error());
    auto groupSet = MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, adapter, "scheduled_ts_adapter.sync_group",
        plan.groupKey.value());
    if (!groupSet) return Result::failure(groupSet.error());
    using MediaGraphBuildSupport::addInputPortChecked;
    using MediaGraphBuildSupport::addOutputPortChecked;
    if (auto status = addInputPortChecked(
            graph, Owner, planSource, "epoch", MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
        !status) return Result::failure(status.error());
    if (auto status = addOutputPortChecked(
            graph, Owner, planSource, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan,
            true, true); !status) return Result::failure(status.error());
    if (auto status = addInputPortChecked(
            graph, Owner, adapter, "plan", MediaStreamKind::Metadata,
            MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan,
            true, false); !status) return Result::failure(status.error());
    if (auto status = addInputPortChecked(
            graph, Owner, adapter, "scheduled", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet,
            true, false); !status) return Result::failure(status.error());
    if (auto status = addOutputPortChecked(
            graph, Owner, adapter, "packet", MediaStreamKind::Any,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::TsAccessUnit,
            true, false); !status) return Result::failure(status.error());
    const auto connect = [&](const MediaEndpoint& from, MediaNodeId to,
                             const char* port, const char* label,
                             const MediaEdgePolicy& policy) {
        return MediaGraphBuildSupport::connectChecked(
            graph, Owner, from.node, from.port, to, port, label, policy);
    };
    for (const auto& [from, to, port, label, policy] : {
             std::tuple{options.epochActivated, planSource, "epoch",
                        "playback epoch -> MPEG-TS plan",
                        plan.edgePolicies.atomicMetadata},
             std::tuple{options.videoCodec, mux, "codec",
                        "video codec -> MPEG-TS mux", plan.edgePolicies.metadata},
             std::tuple{options.audioCodec, mux, "codec",
                        "audio codec -> MPEG-TS mux", plan.edgePolicies.metadata},
             std::tuple{options.scheduled, adapter, "scheduled",
                        "scheduled A/V -> MPEG-TS adapter",
                        plan.edgePolicies.synchronizedPacket}}) {
        auto connected = connect(from, to, port, label, policy);
        if (!connected) return Result::failure(connected.error());
    }
    for (const auto& [to, port, label, policy] : {
             std::tuple{mux, "plan", "MPEG-TS plan -> mux",
                         plan.edgePolicies.atomicMetadata},
             std::tuple{adapter, "plan", "MPEG-TS plan -> adapter",
                         plan.edgePolicies.atomicMetadata}}) {
        auto connected = MediaGraphBuildSupport::connectChecked(
            graph, Owner, planSource, "plan", to, port, label, policy);
        if (!connected) return Result::failure(connected.error());
    }
    auto packetConnected = MediaGraphBuildSupport::connectChecked(
        graph, Owner, adapter, "packet", mux, "packet",
        "scheduled TS AU -> project mux", plan.edgePolicies.synchronizedPacket);
    if (!packetConnected) return Result::failure(packetConnected.error());
    if (rtpSdpPublisher.isValid()) {
        auto connected = MediaGraphBuildSupport::connectChecked(
            graph, Owner, planSource, "plan", rtpSdpPublisher, "plan",
            "MPEG-TS RTP plan -> SDP publisher",
            plan.edgePolicies.atomicMetadata);
        if (!connected) return Result::failure(connected.error());
    }
    return Result::success(
        {planSource, adapter, udpOutput, mux, rtpSdpPublisher});
}

} // namespace media::ffmpeg::graph
