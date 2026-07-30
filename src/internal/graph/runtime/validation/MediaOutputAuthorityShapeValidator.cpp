#include "internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.h"

#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

#include <array>
#include <initializer_list>
#include <string_view>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

bool isLegacyAuthority(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoTimestamp:
    case MediaNodeKind::PacketStartGate:
        return true;
    default:
        return false;
    }
}

::media::Status rejectLegacyAuthorities(const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (isLegacyAuthority(node.kind)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Production synchronized output rejects legacy output and timestamp authorities"));
        }
    }
    return ::media::Status::success();
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

bool validPort(
    const MediaPort* port,
    MediaPortDirection direction,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload) noexcept
{
    return port && port->direction == direction &&
        port->streamKind == stream &&
        port->edgeKind == edge &&
        port->payloadKind == payload;
}

bool hasSingleEdge(
    const MediaGraph& graph,
    MediaPortId from,
    MediaPortId to) noexcept
{
    std::size_t matches = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.from.portId == from && edge.to.portId == to) {
            ++matches;
        }
    }
    return matches == 1;
}

::media::Status validateProjectMux(
    const MediaNode& mux,
    const MediaProjectMpegTsRuntimeOutputPlan& product,
    bool requireByteSink)
{
    if (product.muxSessionKind !=
            MediaMuxSessionKind::ProjectMpegTs ||
        !exactKeys(
            mux.options,
            {MediaTranscodeOptionKey::MuxExpectVideo,
             MediaTranscodeOptionKey::MuxExpectAudio,
             MediaTranscodeOptionKey::MuxSessionKind})) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux node options conflict with its planner product"));
    }
    auto expectVideo = requiredBoolNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxExpectVideo);
    auto expectAudio = requiredBoolNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxExpectAudio);
    auto sessionText = requiredNodeOption(
        &mux.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::MuxSessionKind);
    if (!expectVideo || !expectAudio || !sessionText) {
        return !expectVideo
            ? ::media::Status::failure(expectVideo.error())
            : !expectAudio
                ? ::media::Status::failure(expectAudio.error())
                : ::media::Status::failure(sessionText.error());
    }
    auto session =
        parseMediaMuxSessionKindOption(sessionText.value());
    if (!session || !expectVideo.value() || !expectAudio.value() ||
        session.value() != product.muxSessionKind) {
        return ::media::Status::failure(
            session
                ? ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS mux session and stream set conflict with planner facts")
                : session.error());
    }
    const std::size_t expectedInputs = requireByteSink ? 4 : 3;
    if (mux.inputPorts.size() != expectedInputs ||
        !mux.outputPorts.empty() ||
        !validPort(
            mux.findInputPort("codec"), MediaPortDirection::Input,
            MediaStreamKind::Any, MediaEdgeKind::Metadata,
            MediaPayloadKind::CodecContext) ||
        !validPort(
            mux.findInputPort("packet"), MediaPortDirection::Input,
            MediaStreamKind::Any, MediaEdgeKind::EncodedPacket,
            MediaPayloadKind::TsAccessUnit) ||
        !validPort(
            mux.findInputPort("plan"), MediaPortDirection::Input,
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::ProjectMpegTsRuntimePlan) ||
        (requireByteSink &&
         !validPort(
             mux.findInputPort("resource"),
             MediaPortDirection::Input,
             MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
             MediaPayloadKind::OutputByteSink)) ||
        (!requireByteSink && mux.findInputPort("resource"))) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux ports conflict with its transport product"));
    }
    return ::media::Status::success();
}

::media::Status validateUdpSink(
    const MediaGraph& graph,
    const MediaNode& output,
    const MediaNode& mux,
    const MediaMpegTsUdpOutputPlan& udp)
{
    if (udp.resourceKind != MediaOutputResourceKind::ByteSink ||
        udp.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        !exactKeys(
            output.options,
            {"url", MediaTranscodeOptionKey::OutputResourceKind})) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS UDP sink options conflict with its planner product"));
    }
    auto url = requiredNodeOption(
        &output.options, "MediaOutputAuthorityShapeValidator", "url");
    auto resourceText = requiredNodeOption(
        &output.options, "MediaOutputAuthorityShapeValidator",
        MediaTranscodeOptionKey::OutputResourceKind);
    if (!url || !resourceText) {
        return ::media::Status::failure(
            url ? resourceText.error() : url.error());
    }
    auto resource =
        parseMediaOutputResourceKindOption(resourceText.value());
    const MediaPort* source = output.findOutputPort("resource");
    const MediaPort* target = mux.findInputPort("resource");
    if (!resource || url.value() != udp.url ||
        resource.value() != udp.resourceKind ||
        !output.inputPorts.empty() || output.outputPorts.size() != 1 ||
        !validPort(
            source, MediaPortDirection::Output,
            MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
            MediaPayloadKind::OutputByteSink) ||
        !target ||
        !hasSingleEdge(graph, source->id, target->id)) {
        return ::media::Status::failure(
            resource
                ? ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS UDP sink node conflicts with its exact URL/resource edge product")
                : resource.error());
    }
    return ::media::Status::success();
}

::media::Status validatePlannerProduct(
    const MediaAvSyncRuntimeBinding& binding,
    const MediaProjectMpegTsRuntimeOutputPlan& product)
{
    if (!binding.plan.projectMpegTsOutput ||
        !binding.plan.projectMpegTsOutput->outputMux ||
        binding.plan.rtpOutput ||
        product.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        binding.plan.projectMpegTsOutput->outputMux->parameters() !=
            product.protocol.muxPlan().parameters() ||
        !binding.plan.audioServo.outputSampleRate ||
        *binding.plan.audioServo.outputSampleRate !=
            product.protocol.audioSampleRate()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS runtime output product conflicts with its synchronization planner product"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaOutputAuthorityShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    const MediaAvSyncGraphShape shape(graph);
    if (binding.assemblyMode ==
            MediaAvSyncBindingAssemblyMode::ComponentCore) {
        if (binding.outputAdapter ||
            binding.projectMpegTsOutputProduct) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Component A/V core rejects protocol output products"));
        }
        return shape.requireExact({
            {MediaNodeKind::ScheduledRtpSender, 0,
             "scheduled RTP sender"},
            {MediaNodeKind::DualMediaSdpPublisher, 0,
             "dual-media SDP publisher"},
            {MediaNodeKind::ScheduledTsAccessUnitAdapter, 0,
             "scheduled TS adapter"},
            {MediaNodeKind::ProjectMpegTsPlanSource, 0,
             "Project MPEG-TS plan source"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"},
            {MediaNodeKind::FileMux, 0, "output mux"},
            {MediaNodeKind::FileOutput, 0, "output resource"}},
            "component A/V core output shape");
    }
    if (binding.assemblyMode !=
            MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput ||
        !binding.outputAdapter) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Production A/V output requires its planner adapter"));
    }
    if (auto legacy = rejectLegacyAuthorities(graph); !legacy) {
        return legacy;
    }
    if (*binding.outputAdapter ==
            MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
        if (!binding.plan.rtpOutput ||
            binding.plan.projectMpegTsOutput ||
            binding.projectMpegTsOutputProduct) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Separate RTP output conflicts with planner authority facts"));
        }
        return shape.requireExact({
            {MediaNodeKind::ScheduledRtpSender, 2,
             "scheduled RTP sender"},
            {MediaNodeKind::DualMediaSdpPublisher, 1,
             "dual-media SDP publisher"},
            {MediaNodeKind::ScheduledTsAccessUnitAdapter, 0,
             "scheduled TS adapter"},
            {MediaNodeKind::ProjectMpegTsPlanSource, 0,
             "Project MPEG-TS plan source"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"},
            {MediaNodeKind::FileMux, 0, "Project MPEG-TS mux"},
            {MediaNodeKind::FileOutput, 0, "UDP output resource"}},
            "separate RTP output shape");
    }
    if (*binding.outputAdapter !=
            MediaAvSyncOutputAdapterKind::ProjectMpegTs ||
        !binding.projectMpegTsOutputProduct) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS output requires its complete runtime product"));
    }
    const auto& product =
        *binding.projectMpegTsOutputProduct;
    if (auto exact = validatePlannerProduct(binding, product);
        !exact) {
        return exact;
    }
    auto common = shape.requireExact({
        {MediaNodeKind::ScheduledRtpSender, 0,
         "scheduled RTP sender"},
        {MediaNodeKind::DualMediaSdpPublisher, 0,
         "dual-media SDP publisher"},
        {MediaNodeKind::ScheduledTsAccessUnitAdapter, 1,
         "scheduled TS adapter"},
        {MediaNodeKind::ProjectMpegTsPlanSource, 1,
         "Project MPEG-TS plan source"},
        {MediaNodeKind::FileMux, 1, "Project MPEG-TS mux"}},
        "Project MPEG-TS output shape");
    if (!common) return common;
    const MediaNode& planSource =
        *shape.nodes(
            MediaNodeKind::ProjectMpegTsPlanSource).front();
    auto decoded =
        MediaProjectMpegTsPlanSourceNodePlanCodec::decode(
            planSource);
    if (!decoded) {
        return ::media::Status::failure(decoded.error());
    }
    if (auto exact =
            MediaProjectMpegTsPlanSourceNodePlanCodec::
                validateAgainstPlanner(
                    decoded.value(), binding.groupKey, product);
        !exact) {
        return exact;
    }
    const MediaNode& mux =
        *shape.nodes(MediaNodeKind::FileMux).front();
    if (const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(
                &product.transport)) {
        auto cardinality = shape.requireExact({
            {MediaNodeKind::FileOutput, 1, "UDP output resource"},
            {MediaNodeKind::MpegTsRtpSdpPublisher, 0,
             "MP2T SDP publisher"}},
            "Project MPEG-TS UDP output shape");
        if (!cardinality) return cardinality;
        if (product.protocol.muxPlan().parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP variant conflicts with mux transport"));
        }
        if (auto validMux =
                validateProjectMux(mux, product, true);
            !validMux) {
            return validMux;
        }
        return validateUdpSink(
            graph,
            *shape.nodes(MediaNodeKind::FileOutput).front(),
            mux,
            *udp);
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(
            &product.transport);
    if (!rtp ||
        product.protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP variant conflicts with mux transport"));
    }
    auto cardinality = shape.requireExact({
        {MediaNodeKind::FileOutput, 0, "UDP output resource"},
        {MediaNodeKind::MpegTsRtpSdpPublisher, 1,
         "MP2T SDP publisher"}},
        "Project MPEG-TS RTP output shape");
    if (!cardinality) return cardinality;
    return validateProjectMux(mux, product, false);
}

} // namespace media::ffmpeg::graph
