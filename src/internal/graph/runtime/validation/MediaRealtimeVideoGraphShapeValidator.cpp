#include "internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRealtimeRuntimeBinding.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

bool audioOrAvNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::AudioCodecResolver:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::EncodedAudioCanonicalizer:
    case MediaNodeKind::ScheduledOutputRouter:
        return true;
    default:
        return false;
    }
}

bool legacyOutputNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
        return true;
    default:
        return false;
    }
}

bool exactKeys(
    const MediaNodeOptions& options,
    std::initializer_list<std::string_view> expected)
{
    if (options.values().size() != expected.size()) return false;
    for (std::string_view key : expected) {
        if (!options.has(std::string(key))) return false;
    }
    return true;
}

bool matchesStreamSetOption(
    const MediaNodeOptions& options,
    std::string_view key,
    MediaTranscodeStreamSet expected)
{
    auto encoded = MediaTranscodeStreamSetCodec::encode(expected);
    return encoded && options.has(std::string(key)) &&
        options.value(std::string(key)) == encoded.value();
}

bool validPort(
    const MediaPort* port,
    MediaPortDirection direction,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload) noexcept
{
    return port && port->direction == direction &&
        port->streamKind == stream && port->edgeKind == edge &&
        port->payloadKind == payload;
}

const MediaEdge* singleEdge(
    const MediaGraph& graph,
    MediaPortId from,
    MediaPortId to) noexcept
{
    const MediaEdge* match = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.from.portId != from || edge.to.portId != to) continue;
        if (match) return nullptr;
        match = &edge;
    }
    return match;
}

std::size_t incomingEdgeCount(
    const MediaGraph& graph,
    MediaPortId port) noexcept
{
    std::size_t count = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId == port) ++count;
    }
    return count;
}

const MediaEdge* exactCodecEdge(
    const MediaGraph& graph,
    const MediaPort& input,
    MediaNodeKind sourceKind,
    MediaStreamKind stream,
    const MediaEdgePolicy& policy) noexcept
{
    const MediaEdge* match = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId != input.id) continue;
        if (match) return nullptr;
        match = &edge;
    }
    if (!match || match->policy != policy) return nullptr;
    const MediaNode* source = graph.findNode(match->from.nodeId);
    const MediaPort* port = graph.findPort(match->from.portId);
    return source && source->kind == sourceKind && port &&
            port->nodeId == source->id && port->name == "codec" &&
            validPort(port, MediaPortDirection::Output, stream,
                      MediaEdgeKind::Metadata,
                      MediaPayloadKind::CodecContext)
        ? match
        : nullptr;
}

::media::Status invalid(const char* field)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("Invalid VideoOnly runtime graph shape: ") + field));
}

::media::Status validateRawRtpInput(
    const MediaGraph& graph,
    const std::optional<MediaRealtimeRtpTransportPlan>& transport)
{
    const MediaAvSyncGraphShape shape(graph);
    const auto inputs = shape.nodes(MediaNodeKind::RawRtpInput);
    if (!transport) {
        return inputs.empty()
            ? ::media::Status::success()
            : invalid("raw RTP input without transport product");
    }
    if (inputs.size() != 1) {
        return invalid("raw RTP input cardinality");
    }
    const MediaNodeOptions& options = inputs.front()->options;
    auto senderReportTimeout = requiredPositiveIntNodeOption(
        &options, "RawRtpInputNode", "rtcp.sender_report_timeout_ms");
    auto maximumExtrapolation = requiredPositiveInt64NodeOption(
        &options, "RawRtpInputNode", "rtcp.maximum_extrapolation_ns");
    auto cnameTimeout = requiredPositiveIntNodeOption(
        &options, "RawRtpInputNode", "rtcp.cname_timeout_ms");
    auto requireSenderReports = requiredBoolNodeOption(
        &options, "RawRtpInputNode", "rtcp.require_sender_reports");
    auto requireCname = requiredBoolNodeOption(
        &options, "RawRtpInputNode", "rtcp.require_cname");
    if (!senderReportTimeout || !maximumExtrapolation || !cnameTimeout ||
        !requireSenderReports || !requireCname ||
        senderReportTimeout.value() != transport->senderReportTimeoutMs ||
        maximumExtrapolation.value() !=
            static_cast<std::int64_t>(transport->maximumExtrapolationMs) *
                1'000'000 ||
        cnameTimeout.value() != transport->cnameTimeoutMs ||
        requireSenderReports.value() != transport->requireSenderReports ||
        requireCname.value() != transport->requireCname) {
        return invalid("raw RTP clock liveness options differ from transport product");
    }
    return ::media::Status::success();
}

const MediaEdge* exactEdge(
    const MediaGraph& graph,
    const MediaNode& source,
    const char* sourcePort,
    const MediaNode& target,
    const char* targetPort,
    const MediaEdgePolicy& policy) noexcept
{
    const MediaPort* from = source.findOutputPort(sourcePort);
    const MediaPort* to = target.findInputPort(targetPort);
    if (!from || !to) return nullptr;
    const MediaEdge* edge = singleEdge(graph, from->id, to->id);
    return edge && edge->policy == policy ? edge : nullptr;
}

::media::Status validateLineageEdges(
    const MediaGraph& graph,
    const MediaRealtimeVideoRuntimePlan& runtime)
{
    const MediaAvSyncGraphShape shape(graph);
    const auto demux = shape.nodes(MediaNodeKind::Demux);
    const auto split = shape.nodes(MediaNodeKind::StreamSplit);
    if (demux.size() != split.size() || demux.size() > 1) {
        return invalid("generic ingress cardinality");
    }
    if (!demux.empty() &&
        !exactEdge(graph, *demux.front(), "packet", *split.front(),
                   "packet", runtime.lineageEdgePolicies.ingressPacket)) {
        return invalid("generic ingress packet policy");
    }

    const auto decode = shape.nodes(MediaNodeKind::VideoDecode);
    if (decode.empty()) {
        const auto normalize = shape.nodes(MediaNodeKind::PacketNormalize);
        if (normalize.size() > 1) {
            return invalid("packet-copy normalization cardinality");
        }
        if (!normalize.empty()) {
            const MediaPort* packetInput =
                normalize.front()->findInputPort("packet");
            if (!packetInput ||
                incomingEdgeCount(graph, packetInput->id) != 1) {
                return invalid("packet-copy startup source cardinality");
            }
            const MediaEdge* packetEdge = nullptr;
            for (const MediaEdge& edge : graph.edges()) {
                if (edge.to.portId == packetInput->id) packetEdge = &edge;
            }
            const MediaNode* source = packetEdge
                ? graph.findNode(packetEdge->from.nodeId)
                : nullptr;
            const MediaPort* sourcePort = packetEdge
                ? graph.findPort(packetEdge->from.portId)
                : nullptr;
            if (!source || !sourcePort ||
                ((source->kind != MediaNodeKind::StreamSplit ||
                  sourcePort->name != "video") &&
                 (source->kind != MediaNodeKind::MpegTsDemux ||
                  sourcePort->name != "video") &&
                 (source->kind != MediaNodeKind::RawRtpInput ||
                  sourcePort->name != "packet")) ||
                packetEdge->policy !=
                    runtime.lineageEdgePolicies.startupPacket) {
                return invalid(
                    "packet-copy startup edge source or policy");
            }
        }
        return ::media::Status::success();
    }
    const auto transfer = shape.nodes(MediaNodeKind::HardwareTransfer);
    const auto timestamp = shape.nodes(MediaNodeKind::VideoTimestamp);
    const auto frameRate = shape.nodes(MediaNodeKind::VideoFrameRate);
    const auto filter = shape.nodes(MediaNodeKind::VideoFilter);
    const auto encode = shape.nodes(MediaNodeKind::VideoEncode);
    const auto gate = shape.nodes(MediaNodeKind::PacketStartGate);
    if (decode.size() != 1 || transfer.size() != 1 ||
        timestamp.size() != 1 || frameRate.size() != 1 ||
        filter.size() != 1 || encode.size() != 1 || gate.size() > 1) {
        return invalid("video lineage node cardinality");
    }

    const MediaNode& packetTarget = gate.empty()
        ? *decode.front()
        : *gate.front();
    const MediaPort* packetInput = packetTarget.findInputPort("packet");
    if (!packetInput || incomingEdgeCount(graph, packetInput->id) != 1) {
        return invalid("startup packet source cardinality");
    }
    const MediaEdge* startup = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId == packetInput->id) startup = &edge;
    }
    const MediaNode* startupSource = startup
        ? graph.findNode(startup->from.nodeId)
        : nullptr;
    const MediaPort* startupPort = startup
        ? graph.findPort(startup->from.portId)
        : nullptr;
    const bool validSource = startupSource && startupPort &&
        ((startupSource->kind == MediaNodeKind::StreamSplit &&
          startupPort->name == "video") ||
         (startupSource->kind == MediaNodeKind::MpegTsDemux &&
          startupPort->name == "video") ||
         (startupSource->kind == MediaNodeKind::RawRtpInput &&
          startupPort->name == "packet"));
    if (!validSource ||
        startup->policy != runtime.lineageEdgePolicies.startupPacket) {
        return invalid("startup packet edge source or policy");
    }
    if (!gate.empty() &&
        !exactEdge(graph, *gate.front(), "packet", *decode.front(),
                   "packet", runtime.lineageEdgePolicies.startupPacket)) {
        return invalid("post-gate startup packet policy");
    }

    if (!exactEdge(graph, *decode.front(), "frame", *transfer.front(),
                   "frame", runtime.lineageEdgePolicies.frame) ||
        !exactEdge(graph, *transfer.front(), "frame", *timestamp.front(),
                   "frame", runtime.lineageEdgePolicies.frame) ||
        !exactEdge(graph, *timestamp.front(), "frame", *frameRate.front(),
                   "frame", runtime.lineageEdgePolicies.frame) ||
        !exactEdge(graph, *frameRate.front(), "frame", *filter.front(),
                   "frame", runtime.lineageEdgePolicies.frame) ||
        !exactEdge(graph, *filter.front(), "frame", *encode.front(),
                   "frame", runtime.lineageEdgePolicies.preparedFrame)) {
        return invalid("lossless video frame lineage policy");
    }
    return ::media::Status::success();
}

::media::Status validateScheduler(
    const MediaGraph& graph,
    const MediaNode& scheduler,
    const MediaRealtimeVideoRuntimePlan& runtime)
{
    if (scheduler.inputPorts.size() != 1 ||
        scheduler.outputPorts.size() != 2 ||
        !validPort(
            scheduler.findInputPort("video"), MediaPortDirection::Input,
            MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
            MediaPayloadKind::Packet) ||
        !validPort(
            scheduler.findOutputPort("activation"),
            MediaPortDirection::Output, MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent) ||
        !validPort(
            scheduler.findOutputPort("scheduled_video"),
            MediaPortDirection::Output, MediaStreamKind::Video,
            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet) ||
        scheduler.options.values().size() != 16 ||
        incomingEdgeCount(
            graph, scheduler.findInputPort("video")->id) != 1) {
        return invalid("scheduler ports or cardinality");
    }
    const MediaEdge* schedulerInput = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId == scheduler.findInputPort("video")->id) {
            schedulerInput = &edge;
        }
    }
    if (!schedulerInput ||
        schedulerInput->policy != runtime.edgePolicies.synchronizedPacket) {
        return invalid("scheduler input edge policy");
    }
    auto requireKeyFrame = requiredBoolNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.require_key_frame");
    auto maximumWait = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_wait_ns");
    auto packetCapacity = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.packet_capacity");
    auto maximumUnitBytes = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_unit_bytes");
    auto byteCapacity = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.byte_capacity");
    auto sourceNumerator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.num");
    auto sourceDenominator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.den");
    auto frameRateNumerator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.num");
    auto frameRateDenominator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.den");
    auto packetTimeBaseNumerator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.num");
    auto packetTimeBaseDenominator = requiredPositiveIntNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.den");
    auto packetTimingMode = requiredNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_timing_mode");
    auto transportLead = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.transport_lead_ns");
    auto pacingEnabled = requiredBoolNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.pacing_enabled");
    auto initialGeneration = requiredPositiveInt64NodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.initial_generation");
    auto session = requiredNodeOption(
        &scheduler.options, "MediaVideoOutputSchedulerNode",
        "protocol_output.session");
    const char* expectedTimingMode = runtime.timing.packetTimingMode ==
            MediaRealtimeVideoPacketTimingMode::PacketDuration
        ? "packet_duration"
        : runtime.timing.packetTimingMode ==
                MediaRealtimeVideoPacketTimingMode::PlannedCadence
            ? "planned_cadence"
            : nullptr;
    if (!requireKeyFrame || !maximumWait || !packetCapacity ||
        !maximumUnitBytes || !byteCapacity || !sourceNumerator ||
        !sourceDenominator || !frameRateNumerator ||
        !frameRateDenominator || !packetTimeBaseNumerator ||
        !packetTimeBaseDenominator || !packetTimingMode ||
        !transportLead || !pacingEnabled || !initialGeneration || !session ||
        !expectedTimingMode ||
        requireKeyFrame.value() != runtime.startup.requireKeyFrame ||
        maximumWait.value() != runtime.startup.maximumWait.nanoseconds() ||
        static_cast<std::uint64_t>(packetCapacity.value()) !=
            runtime.startup.packetCapacity ||
        static_cast<std::uint64_t>(maximumUnitBytes.value()) !=
            runtime.startup.maximumUnitBytes ||
        static_cast<std::uint64_t>(byteCapacity.value()) !=
            runtime.startup.byteCapacity ||
        sourceNumerator.value() != runtime.timing.sourceTimeBase.num ||
        sourceDenominator.value() != runtime.timing.sourceTimeBase.den ||
        frameRateNumerator.value() != runtime.timing.outputFrameRate.num ||
        frameRateDenominator.value() != runtime.timing.outputFrameRate.den ||
        packetTimeBaseNumerator.value() !=
            runtime.timing.scheduledPacketTimeBase.num ||
        packetTimeBaseDenominator.value() !=
            runtime.timing.scheduledPacketTimeBase.den ||
        packetTimingMode.value() != expectedTimingMode ||
        transportLead.value() !=
            runtime.scheduling.transportLead.nanoseconds() ||
        pacingEnabled.value() != runtime.scheduling.pacingEnabled ||
        static_cast<std::uint64_t>(initialGeneration.value()) !=
            runtime.scheduling.initialGeneration ||
        session.value() != runtime.sessionKey.value()) {
        return invalid("scheduler options differ from runtime product");
    }
    return ::media::Status::success();
}

::media::Status validateSeparateRtp(
    const MediaGraph& graph,
    const MediaNode& scheduler,
    const MediaRealtimeVideoRuntimePlan& runtime,
    const MediaVideoOnlySeparateRtpOutputRuntimePlan& product)
{
    const MediaAvSyncGraphShape shape(graph);
    auto cardinality = shape.requireExact({
        {MediaNodeKind::ScheduledRtpSender, 1, "video RTP sender"},
        {MediaNodeKind::RtpSdpPublisher, 1, "video SDP publisher"},
        {MediaNodeKind::ProjectMpegTsPlanSource, 0, "MPEG-TS plan source"},
        {MediaNodeKind::ScheduledTsAccessUnitAdapter, 0, "TS adapter"},
        {MediaNodeKind::MpegTsRtpSdpPublisher, 0, "MP2T SDP publisher"},
        {MediaNodeKind::FileMux, 0, "MPEG-TS mux"},
        {MediaNodeKind::FileOutput, 0, "MPEG-TS byte sink"},
        {MediaNodeKind::RtpMux, 0, "legacy RTP mux"},
        {MediaNodeKind::RtpOutput, 0, "legacy RTP output"},
        {MediaNodeKind::SdpWriter, 0, "legacy SDP writer"}},
        "VideoOnly scheduled RTP output");
    if (!cardinality) return cardinality;
    const MediaNode& sender =
        *shape.nodes(MediaNodeKind::ScheduledRtpSender).front();
    const MediaNode& sdp =
        *shape.nodes(MediaNodeKind::RtpSdpPublisher).front();
    auto decoded = MediaScheduledRtpSenderNodePlanCodec::decode(sender);
    if (!decoded) return ::media::Status::failure(decoded.error());
    if (decoded.value().sessionKey != runtime.sessionKey ||
        decoded.value().streamSet != MediaTranscodeStreamSet::VideoOnly ||
        decoded.value().output != product.video ||
        decoded.value().sdp != product.sdp ||
        sender.inputPorts.size() != 3 || sender.outputPorts.size() != 1 ||
        !validPort(sender.findInputPort("activation"),
                   MediaPortDirection::Input, MediaStreamKind::Metadata,
                   MediaEdgeKind::Event, MediaPayloadKind::GraphEvent) ||
        !validPort(sender.findInputPort("codec"), MediaPortDirection::Input,
                   MediaStreamKind::Video, MediaEdgeKind::Metadata,
                   MediaPayloadKind::CodecContext) ||
        !validPort(sender.findInputPort("scheduled"),
                   MediaPortDirection::Input, MediaStreamKind::Video,
                   MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet) ||
        !validPort(sender.findOutputPort("description"),
                   MediaPortDirection::Output, MediaStreamKind::Metadata,
                   MediaEdgeKind::Event, MediaPayloadKind::GraphEvent) ||
        sdp.inputPorts.size() != 1 || !sdp.outputPorts.empty() ||
        !validPort(sdp.findInputPort("video"), MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Event,
                   MediaPayloadKind::GraphEvent) ||
        !exactKeys(sdp.options, {"sdp.path", "sdp.stream_set"}) ||
        sdp.options.value("sdp.path") != product.sdp.path ||
        !matchesStreamSetOption(
            sdp.options, "sdp.stream_set",
            MediaTranscodeStreamSet::VideoOnly)) {
        return invalid("scheduled RTP nodes differ from runtime product");
    }
    const MediaEdge* activation = singleEdge(
        graph, scheduler.findOutputPort("activation")->id,
        sender.findInputPort("activation")->id);
    const MediaEdge* scheduled = singleEdge(
        graph, scheduler.findOutputPort("scheduled_video")->id,
        sender.findInputPort("scheduled")->id);
    const MediaEdge* description = singleEdge(
        graph, sender.findOutputPort("description")->id,
        sdp.findInputPort("video")->id);
    const MediaEdge* codec = exactCodecEdge(
        graph, *sender.findInputPort("codec"), MediaNodeKind::VideoEncode,
        MediaStreamKind::Video, runtime.edgePolicies.metadata);
    if (!activation || !scheduled || !description ||
        !codec ||
        activation->policy != runtime.edgePolicies.atomicMetadata ||
        scheduled->policy != runtime.edgePolicies.atomicVideoPacket ||
        description->policy != runtime.edgePolicies.metadata) {
        return invalid("scheduled RTP edges differ from runtime product");
    }
    return ::media::Status::success();
}

::media::Status validateProjectMpegTs(
    const MediaGraph& graph,
    const MediaNode& scheduler,
    const MediaRealtimeVideoRuntimePlan& runtime,
    const MediaProjectMpegTsRuntimeOutputPlan& product)
{
    const bool udp =
        std::holds_alternative<MediaMpegTsUdpOutputPlan>(product.transport);
    const MediaAvSyncGraphShape shape(graph);
    auto cardinality = shape.requireExact({
        {MediaNodeKind::ScheduledRtpSender, 0, "video RTP sender"},
        {MediaNodeKind::RtpSdpPublisher, 0, "RTP SDP publisher"},
        {MediaNodeKind::ProjectMpegTsPlanSource, 1, "MPEG-TS plan source"},
        {MediaNodeKind::ScheduledTsAccessUnitAdapter, 1, "TS adapter"},
        {MediaNodeKind::MpegTsRtpSdpPublisher, udp ? 0u : 1u,
         "MP2T SDP publisher"},
        {MediaNodeKind::FileMux, 1, "MPEG-TS mux"},
        {MediaNodeKind::FileOutput, udp ? 1u : 0u, "MPEG-TS byte sink"},
        {MediaNodeKind::RtpMux, 0, "legacy RTP mux"},
        {MediaNodeKind::RtpOutput, 0, "legacy RTP output"},
        {MediaNodeKind::SdpWriter, 0, "legacy SDP writer"}},
        "VideoOnly Project MPEG-TS output");
    if (!cardinality) return cardinality;
    if (product.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        !product.protocol.muxPlan().videoOnlyProgram() ||
        product.protocol.muxPlan().audioVideoProgram()) {
        return invalid("MPEG-TS product stream set");
    }
    const MediaNode& source =
        *shape.nodes(MediaNodeKind::ProjectMpegTsPlanSource).front();
    const MediaNode& adapter =
        *shape.nodes(MediaNodeKind::ScheduledTsAccessUnitAdapter).front();
    const MediaNode& mux = *shape.nodes(MediaNodeKind::FileMux).front();
    auto decoded = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(source);
    if (!decoded) return ::media::Status::failure(decoded.error());
    if (auto exact =
            MediaProjectMpegTsPlanSourceNodePlanCodec::validateAgainstPlanner(
                decoded.value(), runtime.sessionKey,
                MediaTranscodeStreamSet::VideoOnly, product);
        !exact) return exact;
    if (source.inputPorts.size() != 1 || source.outputPorts.size() != 1 ||
        !validPort(source.findInputPort("activation"),
                   MediaPortDirection::Input, MediaStreamKind::Metadata,
                   MediaEdgeKind::Event, MediaPayloadKind::GraphEvent) ||
        !validPort(source.findOutputPort("plan"),
                   MediaPortDirection::Output, MediaStreamKind::Metadata,
                   MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan)) {
        return invalid("MPEG-TS plan source differs from runtime product");
    }
    if (adapter.inputPorts.size() != 2 || adapter.outputPorts.size() != 1 ||
        !exactKeys(adapter.options,
                   {"scheduled_ts_adapter.session",
                    "scheduled_ts_adapter.stream_set"}) ||
        adapter.options.value("scheduled_ts_adapter.session") !=
            runtime.sessionKey.value() ||
        !matchesStreamSetOption(
            adapter.options, "scheduled_ts_adapter.stream_set",
            MediaTranscodeStreamSet::VideoOnly) ||
        !validPort(adapter.findInputPort("plan"),
                   MediaPortDirection::Input, MediaStreamKind::Metadata,
                   MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        !validPort(adapter.findInputPort("scheduled"),
                   MediaPortDirection::Input, MediaStreamKind::Video,
                   MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet) ||
        !validPort(adapter.findOutputPort("packet"),
                   MediaPortDirection::Output, MediaStreamKind::Any,
                   MediaEdgeKind::EncodedPacket,
                   MediaPayloadKind::TsAccessUnit)) {
        return invalid("MPEG-TS adapter differs from runtime product");
    }
    auto muxSessionKind = parseMediaMuxSessionKindOption(
        mux.options.value(MediaTranscodeOptionKey::MuxSessionKind));
    if (!exactKeys(mux.options,
                   {MediaTranscodeOptionKey::MuxExpectVideo,
                    MediaTranscodeOptionKey::MuxExpectAudio,
                    MediaTranscodeOptionKey::MuxSessionKind}) ||
        mux.options.value(MediaTranscodeOptionKey::MuxExpectVideo) != "1" ||
        mux.options.value(MediaTranscodeOptionKey::MuxExpectAudio) != "0" ||
        !muxSessionKind ||
        muxSessionKind.value() != MediaMuxSessionKind::ProjectMpegTs) {
        return invalid("MPEG-TS mux options differ from runtime product");
    }
    if (!mux.outputPorts.empty() ||
        mux.inputPorts.size() != (udp ? 4u : 3u)) {
        return invalid("MPEG-TS mux port count differs from runtime product");
    }
    if (!validPort(mux.findInputPort("codec"), MediaPortDirection::Input,
                   MediaStreamKind::Any, MediaEdgeKind::Metadata,
                   MediaPayloadKind::CodecContext) ||
        !validPort(mux.findInputPort("packet"), MediaPortDirection::Input,
                   MediaStreamKind::Any, MediaEdgeKind::EncodedPacket,
                   MediaPayloadKind::TsAccessUnit) ||
        !validPort(mux.findInputPort("plan"), MediaPortDirection::Input,
                   MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan)) {
        return invalid("MPEG-TS mux port types differ from runtime product");
    }
    const MediaEdge* activation = singleEdge(
        graph, scheduler.findOutputPort("activation")->id,
        source.findInputPort("activation")->id);
    const MediaEdge* scheduled = singleEdge(
        graph, scheduler.findOutputPort("scheduled_video")->id,
        adapter.findInputPort("scheduled")->id);
    const MediaEdge* sourceToAdapter = singleEdge(
        graph, source.findOutputPort("plan")->id,
        adapter.findInputPort("plan")->id);
    const MediaEdge* sourceToMux = singleEdge(
        graph, source.findOutputPort("plan")->id,
        mux.findInputPort("plan")->id);
    const MediaEdge* packet = singleEdge(
        graph, adapter.findOutputPort("packet")->id,
        mux.findInputPort("packet")->id);
    const MediaEdge* codec = exactCodecEdge(
        graph, *mux.findInputPort("codec"), MediaNodeKind::VideoEncode,
        MediaStreamKind::Video, runtime.edgePolicies.metadata);
    if (!activation || !scheduled || !sourceToAdapter || !sourceToMux ||
        !packet || !codec ||
        activation->policy != runtime.edgePolicies.atomicMetadata ||
        scheduled->policy != runtime.edgePolicies.synchronizedPacket ||
        sourceToAdapter->policy != runtime.edgePolicies.atomicMetadata ||
        sourceToMux->policy != runtime.edgePolicies.atomicMetadata ||
        packet->policy != runtime.edgePolicies.synchronizedPacket) {
        return invalid("MPEG-TS edges differ from runtime product");
    }
    if (udp) {
        const auto& udpPlan = std::get<MediaMpegTsUdpOutputPlan>(
            product.transport);
        const MediaNode& output =
            *shape.nodes(MediaNodeKind::FileOutput).front();
        if (product.protocol.muxPlan().parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams ||
            udpPlan.resourceKind != MediaOutputResourceKind::ByteSink ||
            udpPlan.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
            !exactKeys(output.options,
                       {"url", MediaTranscodeOptionKey::OutputResourceKind}) ||
            output.options.value("url") != udpPlan.url ||
            output.options.value(MediaTranscodeOptionKey::OutputResourceKind) !=
                "byte_sink" ||
            !output.inputPorts.empty() || output.outputPorts.size() != 1 ||
            !validPort(output.findOutputPort("resource"),
                       MediaPortDirection::Output, MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::OutputByteSink) ||
            !validPort(mux.findInputPort("resource"),
                       MediaPortDirection::Input, MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::OutputByteSink) ||
            !singleEdge(graph, output.findOutputPort("resource")->id,
                        mux.findInputPort("resource")->id)) {
            return invalid("MPEG-TS UDP transport differs from runtime product");
        }
        return ::media::Status::success();
    }
    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
        &product.transport);
    const MediaNode& publisher =
        *shape.nodes(MediaNodeKind::MpegTsRtpSdpPublisher).front();
    if (!rtp ||
        product.protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp ||
        !exactKeys(publisher.options,
                   {"mpegts_rtp_sdp.session",
                    "mpegts_rtp_sdp.stream_set"}) ||
        publisher.options.value("mpegts_rtp_sdp.session") !=
            runtime.sessionKey.value() ||
        !matchesStreamSetOption(
            publisher.options, "mpegts_rtp_sdp.stream_set",
            MediaTranscodeStreamSet::VideoOnly) ||
        publisher.inputPorts.size() != 1 ||
        !publisher.outputPorts.empty() ||
        !validPort(publisher.findInputPort("plan"),
                   MediaPortDirection::Input, MediaStreamKind::Metadata,
                   MediaEdgeKind::Metadata,
                   MediaPayloadKind::ProjectMpegTsRuntimePlan)) {
        return invalid("MPEG-TS RTP transport differs from runtime product");
    }
    const MediaEdge* planToPublisher = singleEdge(
        graph, source.findOutputPort("plan")->id,
        publisher.findInputPort("plan")->id);
    return planToPublisher &&
            planToPublisher->policy == runtime.edgePolicies.atomicMetadata
        ? ::media::Status::success()
        : invalid("MPEG-TS RTP SDP edge differs from runtime product");
}

} // namespace

::media::Status MediaRealtimeVideoGraphShapeValidator::validate(
    const MediaGraph& graph,
    const MediaRealtimeVideoRuntimeBinding& binding)
{
    const auto& runtime = binding.runtime;
    if (!runtime.sessionKey.valid()) return invalid("protocol session");
    if (auto valid = validateRawRtpInput(graph, binding.inputTransport);
        !valid) {
        return valid;
    }
    if (auto valid = validateLineageEdges(graph, runtime); !valid) {
        return valid;
    }
    const MediaNode* scheduler = nullptr;
    for (const MediaNode& node : graph.nodes()) {
        if (audioOrAvNode(node.kind)) return invalid("audio or A/V node");
        if (legacyOutputNode(node.kind)) return invalid("legacy output node");
        if (node.kind == MediaNodeKind::VideoOutputScheduler) {
            if (scheduler) return invalid("duplicate video scheduler");
            scheduler = &node;
        }
        for (const MediaPort& port : node.inputPorts) {
            if (port.streamKind == MediaStreamKind::Audio) {
                return invalid("audio input port");
            }
        }
        for (const MediaPort& port : node.outputPorts) {
            if (port.streamKind == MediaStreamKind::Audio) {
                return invalid("audio output port");
            }
        }
    }
    if (!scheduler) return invalid("missing video scheduler");
    if (auto valid = validateScheduler(graph, *scheduler, runtime); !valid) {
        return valid;
    }
    if (const auto* separate =
            std::get_if<MediaVideoOnlySeparateRtpOutputRuntimePlan>(
                &runtime.outputAdapter)) {
        return validateSeparateRtp(graph, *scheduler, runtime, *separate);
    }
    if (const auto* mpegTs =
            std::get_if<MediaProjectMpegTsRuntimeOutputPlan>(
                &runtime.outputAdapter)) {
        return validateProjectMpegTs(graph, *scheduler, runtime, *mpegTs);
    }
    return invalid("unknown output adapter variant");
}

::media::Status MediaRealtimeVideoGraphShapeValidator::validateAbsent(
    const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoOutputScheduler) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Non-VideoOnly graph rejects a video scheduler"));
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
