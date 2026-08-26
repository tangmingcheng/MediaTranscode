#include "internal/graph/nodes/output/MediaRtpDatagramMaterializerNodePlanCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationModeCodec.h"

#include <array>
#include <charconv>
#include <limits>
#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaRtpDatagramMaterializerNodePlanCodec";
constexpr const char* NodeName = "MediaRtpDatagramMaterializerNode";
constexpr std::array<const char*, 33> OptionKeys{
    "scheduled_rtp.session",
    "scheduled_rtp.stream_set",
    "scheduled_rtp.stream",
    "scheduled_rtp.transport.address_family",
    "scheduled_rtp.transport.remote_address",
    "scheduled_rtp.transport.remote_rtp_port",
    "scheduled_rtp.transport.remote_rtcp_port",
    "scheduled_rtp.packetization.codec",
    "scheduled_rtp.packetization.time_base_num",
    "scheduled_rtp.packetization.time_base_den",
    "scheduled_rtp.packetization.mode",
    "scheduled_rtp.packetization.payload_type",
    "scheduled_rtp.packetization.maximum_datagram_bytes",
    "scheduled_rtp.packetization.maximum_access_unit_samples",
    "scheduled_rtp.packetization.maximum_access_unit_payload_bytes",
    "scheduled_rtp.packetization.maximum_datagrams_per_access_unit",
    "scheduled_rtp.packetization.emission_authority",
    "scheduled_rtp.packetization.packet_layout",
    "scheduled_rtp.packetization.length_field_bytes",
    "scheduled_rtp.ssrc",
    "scheduled_rtp.base_timestamp",
    "scheduled_rtp.clock_rate",
    "scheduled_rtp.cname",
    "scheduled_rtp.sender_lead_ns",
    "scheduled_rtp.sender_report_interval_ns",
    "scheduled_rtp.sdp.path",
    "scheduled_rtp.sdp.origin_username",
    "scheduled_rtp.sdp.session_name",
    "scheduled_rtp.sdp.origin_address_family",
    "scheduled_rtp.sdp.origin_numeric_address",
    "scheduled_rtp.sdp.cname",
    "scheduled_rtp.sdp.session_id_policy",
    "scheduled_rtp.sdp.session_version_policy"};

bool hasExactOptionKeys(const MediaNodeOptions& options)
{
    if (options.values().size() != OptionKeys.size()) return false;
    for (const char* key : OptionKeys) {
        if (!options.has(key)) return false;
    }
    return true;
}

const char* familyName(MediaIpAddressFamily family) noexcept
{
    return family == MediaIpAddressFamily::Ipv4 ? "ipv4" : "ipv6";
}

::media::Result<MediaIpAddressFamily> parseFamily(
    const MediaNodeOptions& options,
    const char* key)
{
    auto text = requiredNodeOption(&options, NodeName, key);
    if (!text) {
        return ::media::Result<MediaIpAddressFamily>::failure(text.error());
    }
    if (text.value() == "ipv4") {
        return ::media::Result<MediaIpAddressFamily>::success(
            MediaIpAddressFamily::Ipv4);
    }
    if (text.value() == "ipv6") {
        return ::media::Result<MediaIpAddressFamily>::success(
            MediaIpAddressFamily::Ipv6);
    }
    return ::media::Result<MediaIpAddressFamily>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender requires an explicit IP address family"));
}

template <typename Unsigned>
::media::Result<Unsigned> parseUnsigned(
    const MediaNodeOptions& options,
    const char* key,
    bool allowZero = false)
{
    auto text = requiredNodeOption(&options, NodeName, key);
    if (!text) return ::media::Result<Unsigned>::failure(text.error());
    unsigned long long value = 0;
    const char* begin = text.value().data();
    const char* end = begin + text.value().size();
    const auto parsed = std::from_chars(begin, end, value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        (!allowZero && value == 0) ||
        value > static_cast<unsigned long long>(
            (std::numeric_limits<Unsigned>::max)())) {
        return ::media::Result<Unsigned>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(NodeName) + " requires unsigned option: " + key));
    }
    return ::media::Result<Unsigned>::success(
        static_cast<Unsigned>(value));
}

::media::Status setOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const std::initializer_list<std::pair<std::string, std::string>>& values)
{
    for (const auto& [key, value] : values) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, nodeId, key, value);
        if (!set) return ::media::Status::failure(set.error());
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRtpDatagramMaterializerNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaProtocolOutputSessionKey& sessionKey,
    MediaTranscodeStreamSet streamSet,
    const MediaScheduledRtpOutputPlan& output,
    const MediaSeparateRtpSdpRuntimePlan& sdp)
{
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    if (!sessionKey.valid() || !graph.findNode(nodeId) ||
        !encodedStreamSet ||
        (streamSet == MediaTranscodeStreamSet::VideoOnly &&
         output.stream != MediaScheduledStream::Video) ||
        output.transport.remoteRtpEndpoint().numericAddress() !=
            output.transport.remoteRtcpEndpoint().numericAddress() ||
        sdp.sessionIdPolicy != MediaRtpSdpSessionIdPolicy::SharedNtpEpoch ||
        sdp.sessionVersionPolicy !=
            MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration ||
        output.cname != sdp.cname) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP node options require one complete planned sender"));
    }
    return setOptions(graph, nodeId, {
        {"scheduled_rtp.session", sessionKey.value()},
        {"scheduled_rtp.stream_set", std::string(encodedStreamSet.value())},
        {"scheduled_rtp.stream", output.stream == MediaScheduledStream::Video
             ? "video" : "audio"},
        {"scheduled_rtp.transport.address_family",
             familyName(output.transport.addressFamily())},
        {"scheduled_rtp.transport.remote_address",
             output.transport.remoteRtpEndpoint().numericAddress()},
        {"scheduled_rtp.transport.remote_rtp_port",
             std::to_string(output.transport.remoteRtpEndpoint().port())},
        {"scheduled_rtp.transport.remote_rtcp_port",
             std::to_string(output.transport.remoteRtcpEndpoint().port())},
        {"scheduled_rtp.packetization.codec",
             output.packetization.codecName()},
        {"scheduled_rtp.packetization.time_base_num",
             std::to_string(output.packetization.streamTimeBaseNumerator())},
        {"scheduled_rtp.packetization.time_base_den",
             std::to_string(output.packetization.streamTimeBaseDenominator())},
        {"scheduled_rtp.packetization.mode",
             std::string(MediaScheduledRtpPacketizationModeCodec::encode(
                 output.packetization.packetizationMode()))},
        {"scheduled_rtp.packetization.payload_type",
             std::to_string(output.packetization.payloadType())},
        {"scheduled_rtp.packetization.maximum_datagram_bytes",
             std::to_string(output.packetization.maximumDatagramBytes())},
        {"scheduled_rtp.packetization.maximum_access_unit_samples",
             output.packetization.maximumAccessUnitSamples()
                 ? std::to_string(*output.packetization.maximumAccessUnitSamples())
                 : "none"},
        {"scheduled_rtp.packetization.maximum_access_unit_payload_bytes",
             std::to_string(output.packetization.emissionContract()
                                .maximumAccessUnitPayloadBytes())},
        {"scheduled_rtp.packetization.maximum_datagrams_per_access_unit",
             std::to_string(output.packetization.emissionContract()
                                .maximumDatagramsPerAccessUnit())},
        {"scheduled_rtp.packetization.emission_authority",
             output.packetization.emissionContract().authority()},
        {"scheduled_rtp.packetization.packet_layout",
             !output.packetization.emissionContract().packetLayout()
                 ? "none"
                 : output.packetization.emissionContract().packetLayout()->kind() ==
                           MediaEncodedPacketLayoutKind::StartCodeDelimited
                       ? "start-code" : "length-prefixed"},
        {"scheduled_rtp.packetization.length_field_bytes",
             output.packetization.emissionContract().packetLayout() &&
                     output.packetization.emissionContract().packetLayout()
                         ->lengthFieldBytes()
                 ? std::to_string(*output.packetization.emissionContract()
                                       .packetLayout()->lengthFieldBytes())
                 : "none"},
        {"scheduled_rtp.ssrc", std::to_string(output.ssrc)},
        {"scheduled_rtp.base_timestamp", std::to_string(output.baseTimestamp)},
        {"scheduled_rtp.clock_rate", std::to_string(output.clockRate)},
        {"scheduled_rtp.cname", output.cname},
        {"scheduled_rtp.sender_lead_ns",
             std::to_string(output.senderLead.nanoseconds())},
        {"scheduled_rtp.sender_report_interval_ns",
             std::to_string(output.senderReportInterval.nanoseconds())},
        {"scheduled_rtp.sdp.path", sdp.path},
        {"scheduled_rtp.sdp.origin_username", sdp.originUsername},
        {"scheduled_rtp.sdp.session_name", sdp.sessionName},
        {"scheduled_rtp.sdp.origin_address_family",
             familyName(sdp.originAddressFamily)},
        {"scheduled_rtp.sdp.origin_numeric_address",
             sdp.originNumericAddress},
        {"scheduled_rtp.sdp.cname", sdp.cname},
        {"scheduled_rtp.sdp.session_id_policy", "shared_ntp_epoch"},
        {"scheduled_rtp.sdp.session_version_policy",
             "active_playback_generation"}
    });
}

::media::Result<MediaDecodedRtpDatagramMaterializerNodePlan>
MediaRtpDatagramMaterializerNodePlanCodec::decode(const MediaNode& node)
{
    using DecodedResult =
        ::media::Result<MediaDecodedRtpDatagramMaterializerNodePlan>;
    if (node.kind != MediaNodeKind::RtpDatagramMaterializer ||
        !hasExactOptionKeys(node.options)) {
        return DecodedResult::failure(::media::ErrorInfo::invalidArgument(
            "RTP datagram materializer plan decoder requires its kind and exact option set"));
    }
    auto sessionText = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.session");
    auto streamSetText = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.stream_set");
    auto streamText = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.stream");
    auto transportFamily = parseFamily(
        node.options, "scheduled_rtp.transport.address_family");
    auto remoteAddress = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.transport.remote_address");
    auto remoteRtpPort = parseUnsigned<std::uint16_t>(
        node.options, "scheduled_rtp.transport.remote_rtp_port");
    auto remoteRtcpPort = parseUnsigned<std::uint16_t>(
        node.options, "scheduled_rtp.transport.remote_rtcp_port");
    if (!sessionText || !streamSetText || !streamText || !transportFamily ||
        !remoteAddress || !remoteRtpPort || !remoteRtcpPort) {
        const ::media::ErrorInfo error = !sessionText ? sessionText.error()
            : !streamSetText ? streamSetText.error()
            : !streamText ? streamText.error()
            : !transportFamily ? transportFamily.error()
            : !remoteAddress ? remoteAddress.error()
            : !remoteRtpPort ? remoteRtpPort.error()
            : remoteRtcpPort.error();
        return DecodedResult::failure(error);
    }
    MediaScheduledStream stream;
    MediaStreamKind streamKind;
    if (streamText.value() == "video") {
        stream = MediaScheduledStream::Video;
        streamKind = MediaStreamKind::Video;
    } else if (streamText.value() == "audio") {
        stream = MediaScheduledStream::Audio;
        streamKind = MediaStreamKind::Audio;
    } else {
        return DecodedResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender rejects an unknown stream"));
    }
    auto transport = MediaRtpRemoteEndpointPair::create(
        transportFamily.value(), remoteAddress.value(),
        remoteRtpPort.value(), remoteRtcpPort.value());
    if (!transport) return DecodedResult::failure(transport.error());

    auto codec = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.packetization.codec");
    auto timeBaseNum = parseUnsigned<int>(
        node.options, "scheduled_rtp.packetization.time_base_num");
    auto timeBaseDen = parseUnsigned<int>(
        node.options, "scheduled_rtp.packetization.time_base_den");
    auto mode = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.packetization.mode");
    auto payloadType = parseUnsigned<int>(
        node.options, "scheduled_rtp.packetization.payload_type", true);
    auto packetMaximum = parseUnsigned<std::size_t>(
        node.options, "scheduled_rtp.packetization.maximum_datagram_bytes");
    auto samplesText = requiredNodeOption(
        &node.options, NodeName,
        "scheduled_rtp.packetization.maximum_access_unit_samples");
    auto maximumAccessUnitBytes = parseUnsigned<std::uint64_t>(
        node.options,
        "scheduled_rtp.packetization.maximum_access_unit_payload_bytes");
    auto encodedMaximumDatagrams = parseUnsigned<std::uint64_t>(
        node.options,
        "scheduled_rtp.packetization.maximum_datagrams_per_access_unit");
    auto emissionAuthority = requiredNodeOption(
        &node.options, NodeName,
        "scheduled_rtp.packetization.emission_authority");
    auto packetLayoutText = requiredNodeOption(
        &node.options, NodeName,
        "scheduled_rtp.packetization.packet_layout");
    auto lengthFieldText = requiredNodeOption(
        &node.options, NodeName,
        "scheduled_rtp.packetization.length_field_bytes");
    if (!codec || !timeBaseNum || !timeBaseDen || !mode || !payloadType ||
        !packetMaximum || !samplesText || !maximumAccessUnitBytes ||
        !encodedMaximumDatagrams || !emissionAuthority || !packetLayoutText ||
        !lengthFieldText) {
        const ::media::ErrorInfo error = !codec ? codec.error()
            : !timeBaseNum ? timeBaseNum.error()
            : !timeBaseDen ? timeBaseDen.error()
            : !mode ? mode.error()
            : !payloadType ? payloadType.error()
            : !packetMaximum ? packetMaximum.error()
            : !samplesText ? samplesText.error()
            : !maximumAccessUnitBytes ? maximumAccessUnitBytes.error()
            : !encodedMaximumDatagrams ? encodedMaximumDatagrams.error()
            : !emissionAuthority ? emissionAuthority.error()
            : !packetLayoutText ? packetLayoutText.error()
            : lengthFieldText.error();
        return DecodedResult::failure(error);
    }
    std::optional<int> maximumSamples;
    if (samplesText.value() != "none") {
        auto parsedSamples = parseUnsigned<int>(
            node.options,
            "scheduled_rtp.packetization.maximum_access_unit_samples");
        if (!parsedSamples) return DecodedResult::failure(parsedSamples.error());
        maximumSamples = parsedSamples.value();
    }
    auto decodedMode = MediaScheduledRtpPacketizationModeCodec::decode(
        mode.value());
    if (!decodedMode) return DecodedResult::failure(decodedMode.error());
    ::media::Result<MediaRtpAccessUnitEmissionContract> emission =
        ::media::Result<MediaRtpAccessUnitEmissionContract>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP emission contract layout is invalid"));
    if (streamKind == MediaStreamKind::Audio &&
        packetLayoutText.value() == "none" &&
        lengthFieldText.value() == "none") {
        emission = MediaRtpAccessUnitEmissionContract::createAacLatm(
            maximumAccessUnitBytes.value(), packetMaximum.value(),
            emissionAuthority.value());
    } else if (streamKind == MediaStreamKind::Video) {
        std::optional<MediaEncodedPacketLayout> layout;
        if (packetLayoutText.value() == "start-code" &&
            lengthFieldText.value() == "none") {
            layout = MediaEncodedPacketLayout::startCodeDelimited();
        } else if (packetLayoutText.value() == "length-prefixed" &&
                   lengthFieldText.value() != "none") {
            auto width = parseUnsigned<std::uint8_t>(
                node.options,
                "scheduled_rtp.packetization.length_field_bytes");
            if (!width) return DecodedResult::failure(width.error());
            auto created = MediaEncodedPacketLayout::lengthPrefixed(width.value());
            if (!created) return DecodedResult::failure(created.error());
            layout = created.value();
        }
        if (layout) {
            emission = MediaRtpAccessUnitEmissionContract::createVideo(
                decodedMode.value(), *layout, maximumAccessUnitBytes.value(),
                packetMaximum.value(), emissionAuthority.value());
        }
    }
    if (!emission || emission.value().maximumDatagramsPerAccessUnit() !=
                         encodedMaximumDatagrams.value()) {
        return DecodedResult::failure(
            emission ? ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP emission geometry was altered")
                     : emission.error());
    }
    auto packetization = MediaScheduledRtpPacketizationPlan::create(
        streamKind, codec.value(), timeBaseNum.value(), timeBaseDen.value(),
        payloadType.value(), packetMaximum.value(),
        std::move(emission).value(), maximumSamples);
    if (!packetization) return DecodedResult::failure(packetization.error());
    if (
        decodedMode.value() != packetization.value().packetizationMode()) {
        return DecodedResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP packetization options contradict the planned transport"));
    }

    auto ssrc = parseUnsigned<std::uint32_t>(node.options, "scheduled_rtp.ssrc");
    auto baseTimestamp = parseUnsigned<std::uint32_t>(
        node.options, "scheduled_rtp.base_timestamp", true);
    auto clockRate = parseUnsigned<int>(node.options, "scheduled_rtp.clock_rate");
    auto cname = requiredNodeOption(&node.options, NodeName, "scheduled_rtp.cname");
    auto senderLead = requiredPositiveInt64NodeOption(
        &node.options, NodeName, "scheduled_rtp.sender_lead_ns");
    auto reportInterval = requiredPositiveInt64NodeOption(
        &node.options, NodeName, "scheduled_rtp.sender_report_interval_ns");
    auto sdpPath = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.path");
    auto originUsername = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.origin_username");
    auto sessionName = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.session_name");
    auto originFamily = parseFamily(
        node.options, "scheduled_rtp.sdp.origin_address_family");
    auto originAddress = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.origin_numeric_address");
    auto sdpCname = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.cname");
    auto sessionIdPolicy = requiredNodeOption(
        &node.options, NodeName, "scheduled_rtp.sdp.session_id_policy");
    auto sessionVersionPolicy = requiredNodeOption(
        &node.options, NodeName,
        "scheduled_rtp.sdp.session_version_policy");
    if (!ssrc || !baseTimestamp || !clockRate || !cname || !senderLead ||
        !reportInterval || !sdpPath || !originUsername || !sessionName ||
        !originFamily || !originAddress || !sdpCname || !sessionIdPolicy ||
        !sessionVersionPolicy) {
        const ::media::ErrorInfo error = !ssrc ? ssrc.error()
            : !baseTimestamp ? baseTimestamp.error()
            : !clockRate ? clockRate.error()
            : !cname ? cname.error()
            : !senderLead ? senderLead.error()
            : !reportInterval ? reportInterval.error()
            : !sdpPath ? sdpPath.error()
            : !originUsername ? originUsername.error()
            : !sessionName ? sessionName.error()
            : !originFamily ? originFamily.error()
            : !originAddress ? originAddress.error()
            : !sdpCname ? sdpCname.error()
            : !sessionIdPolicy ? sessionIdPolicy.error()
            : sessionVersionPolicy.error();
        return DecodedResult::failure(error);
    }
    if (sessionIdPolicy.value() != "shared_ntp_epoch" ||
        sessionVersionPolicy.value() != "active_playback_generation" ||
        cname.value() != sdpCname.value()) {
        return DecodedResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP SDP options contradict their planned policies"));
    }
    MediaProtocolOutputSessionKey sessionKey(sessionText.value());
    auto streamSet = MediaTranscodeStreamSetCodec::decode(
        streamSetText.value());
    if (!streamSet) return DecodedResult::failure(streamSet.error());
    if (!sessionKey.valid() ||
        (streamSet.value() == MediaTranscodeStreamSet::VideoOnly &&
         stream != MediaScheduledStream::Video)) {
        return DecodedResult::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender requires a valid session and matching stream set"));
    }
    MediaScheduledRtpOutputPlan output{
        stream,
        std::move(transport).value(),
        std::move(packetization).value(),
        ssrc.value(),
        baseTimestamp.value(),
        clockRate.value(),
        cname.value(),
        MediaRunningTime::fromNanoseconds(senderLead.value()),
        MediaRunningTime::fromNanoseconds(reportInterval.value())};
    MediaSeparateRtpSdpRuntimePlan sdp{
        sdpPath.value(),
        originUsername.value(),
        sessionName.value(),
        originFamily.value(),
        originAddress.value(),
        sdpCname.value(),
        MediaRtpSdpSessionIdPolicy::SharedNtpEpoch,
        MediaRtpSdpSessionVersionPolicy::ActivePlaybackGeneration};
    return DecodedResult::success(MediaDecodedRtpDatagramMaterializerNodePlan{
        std::move(sessionKey), std::move(streamSet).value(),
        std::move(output), std::move(sdp)});
}

} // namespace media::ffmpeg::graph
