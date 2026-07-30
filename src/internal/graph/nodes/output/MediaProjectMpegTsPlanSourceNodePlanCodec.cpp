#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <array>
#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaProjectMpegTsPlanSourceNodePlanCodec";
constexpr const char* GroupKey = "project_mpeg_ts_plan.sync_group";
constexpr const char* PlanKey = "project_mpeg_ts_plan.parameters";
constexpr const char* AudioSampleRateKey =
    "project_mpeg_ts_plan.audio_sample_rate";
constexpr const char* VariantKey =
    "project_mpeg_ts_plan.transport.variant";
constexpr const char* MuxSessionKindKey =
    "project_mpeg_ts_plan.mux_session_kind";
constexpr std::size_t MuxFieldCount = 32;

constexpr std::array<const char*, 8> UdpKeys{
    GroupKey,
    PlanKey,
    AudioSampleRateKey,
    VariantKey,
    "project_mpeg_ts_plan.transport.udp.url",
    "project_mpeg_ts_plan.transport.udp.resource_kind",
    "project_mpeg_ts_plan.transport.udp.mux_session_kind",
    MuxSessionKindKey};

constexpr std::array<const char*, 31> RtpKeys{
    GroupKey,
    PlanKey,
    AudioSampleRateKey,
    VariantKey,
    "project_mpeg_ts_plan.transport.rtp.address_family",
    "project_mpeg_ts_plan.transport.rtp.local_address",
    "project_mpeg_ts_plan.transport.rtp.remote_rtp_address",
    "project_mpeg_ts_plan.transport.rtp.remote_rtcp_address",
    "project_mpeg_ts_plan.transport.rtp.remote_rtp_port",
    "project_mpeg_ts_plan.transport.rtp.remote_rtcp_port",
    "project_mpeg_ts_plan.transport.rtp.local_port_policy",
    "project_mpeg_ts_plan.transport.rtp.local_rtp_port",
    "project_mpeg_ts_plan.transport.rtp.local_rtcp_port",
    "project_mpeg_ts_plan.transport.rtp.send_buffer_bytes",
    "project_mpeg_ts_plan.transport.rtp.maximum_datagram_bytes",
    "project_mpeg_ts_plan.transport.rtp.io_behavior",
    "project_mpeg_ts_plan.transport.rtp.payload_type",
    "project_mpeg_ts_plan.transport.rtp.clock_rate",
    "project_mpeg_ts_plan.transport.rtp.ssrc",
    "project_mpeg_ts_plan.transport.rtp.base_timestamp",
    "project_mpeg_ts_plan.transport.rtp.cname",
    "project_mpeg_ts_plan.transport.rtp.sender_report_interval_ns",
    "project_mpeg_ts_plan.transport.rtp.ts_packets_per_payload",
    "project_mpeg_ts_plan.transport.rtp.sdp.path",
    "project_mpeg_ts_plan.transport.rtp.sdp.origin_username",
    "project_mpeg_ts_plan.transport.rtp.sdp.session_name",
    "project_mpeg_ts_plan.transport.rtp.sdp.origin_address_family",
    "project_mpeg_ts_plan.transport.rtp.sdp.origin_numeric_address",
    "project_mpeg_ts_plan.transport.rtp.sdp.cname",
    "project_mpeg_ts_plan.transport.rtp.initial_sequence_number",
    MuxSessionKindKey};

template <typename Value>
::media::Result<Value> narrow(std::uint64_t value)
{
    if (value > static_cast<std::uint64_t>(
                    (std::numeric_limits<Value>::max)())) {
        return ::media::Result<Value>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS plan option contains an out-of-range field"));
    }
    return ::media::Result<Value>::success(static_cast<Value>(value));
}

template <typename Value>
::media::Result<Value> parseUnsignedOption(
    const MediaNodeOptions& options,
    const char* key,
    bool allowZero = false)
{
    auto text = requiredNodeOption(&options, Owner, key);
    if (!text) return ::media::Result<Value>::failure(text.error());
    std::uint64_t value = 0;
    const char* begin = text.value().data();
    const char* end = begin + text.value().size();
    const auto parsed = std::from_chars(begin, end, value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        (!allowZero && value == 0)) {
        return ::media::Result<Value>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Project MPEG-TS plan requires unsigned option: ") +
                key));
    }
    return narrow<Value>(value);
}

::media::Result<std::array<std::uint64_t, MuxFieldCount>>
parseMuxFields(std::string_view text)
{
    std::array<std::uint64_t, MuxFieldCount> fields{};
    std::size_t count = 0;
    while (!text.empty() && count < fields.size()) {
        const auto separator = text.find(',');
        const auto token = text.substr(0, separator);
        if (token.empty()) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS mux plan contains an empty field"));
        }
        const char* begin = token.data();
        const char* end = begin + token.size();
        const auto parsed =
            std::from_chars(begin, end, fields[count], 10);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS mux plan contains a non-numeric field"));
        }
        ++count;
        if (separator == std::string_view::npos) {
            text = {};
        } else {
            text.remove_prefix(separator + 1);
        }
    }
    if (count != fields.size() || !text.empty()) {
        return ::media::Result<decltype(fields)>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux plan has the wrong field count"));
    }
    return ::media::Result<decltype(fields)>::success(fields);
}

std::string encodeMux(const MediaTsMuxPlan& muxPlan)
{
    const auto& p = muxPlan.parameters();
    std::ostringstream encoded;
    encoded << p.transportStreamId << ',' << p.programNumber << ','
            << p.patPid << ',' << p.programMapPid << ',' << p.videoPid << ','
            << p.audioPid << ',' << p.pcrPid << ','
            << static_cast<unsigned>(p.tableVersion) << ','
            << p.psiRepeatInterval.nanoseconds() << ','
            << static_cast<unsigned>(p.videoStreamType) << ','
            << static_cast<unsigned>(p.audioStreamType) << ','
            << static_cast<unsigned>(p.h264InputLayout) << ','
            << static_cast<unsigned>(p.h264NalLengthBytes) << ','
            << static_cast<unsigned>(p.parameterSetPolicy) << ','
            << static_cast<unsigned>(p.aac.mpegId) << ','
            << static_cast<unsigned>(p.aac.audioObjectType) << ','
            << static_cast<unsigned>(p.aac.samplingFrequencyIndex) << ','
            << static_cast<unsigned>(p.aac.channelConfiguration) << ','
            << p.clock.pcrInterval.nanoseconds() << ','
            << p.clock.maximumPcrGap.nanoseconds() << ','
            << p.clock.maximumPcrJitter.nanoseconds() << ','
            << p.clock.timestampTimeBaseNumerator << ','
            << p.clock.timestampTimeBaseDenominator << ','
            << p.transportDecodeLead.nanoseconds() << ',' << p.packetSize << ','
            << static_cast<unsigned>(p.continuity.pat) << ','
            << static_cast<unsigned>(p.continuity.pmt) << ','
            << static_cast<unsigned>(p.continuity.video) << ','
            << static_cast<unsigned>(p.continuity.audio) << ','
            << static_cast<unsigned>(p.maximumPacketsPerDatagram) << ','
            << static_cast<unsigned>(p.transportKind) << ','
            << p.maximumAudioAccessUnitSamples;
    return encoded.str();
}

::media::Result<MediaTsMuxPlan> decodeMux(std::string_view text)
{
    auto parsed = parseMuxFields(text);
    if (!parsed) {
        return ::media::Result<MediaTsMuxPlan>::failure(parsed.error());
    }
    const auto& f = parsed.value();
    auto tsid = narrow<std::uint16_t>(f[0]);
    auto program = narrow<std::uint16_t>(f[1]);
    auto pat = narrow<std::uint16_t>(f[2]);
    auto pmt = narrow<std::uint16_t>(f[3]);
    auto videoPid = narrow<std::uint16_t>(f[4]);
    auto audioPid = narrow<std::uint16_t>(f[5]);
    auto pcrPid = narrow<std::uint16_t>(f[6]);
    auto table = narrow<std::uint8_t>(f[7]);
    auto videoType = narrow<std::uint8_t>(f[9]);
    auto audioType = narrow<std::uint8_t>(f[10]);
    auto nalBytes = narrow<std::uint8_t>(f[12]);
    auto aacMpeg = narrow<std::uint8_t>(f[14]);
    auto aacObject = narrow<std::uint8_t>(f[15]);
    auto aacFrequency = narrow<std::uint8_t>(f[16]);
    auto aacChannels = narrow<std::uint8_t>(f[17]);
    auto packetSize = narrow<std::uint16_t>(f[24]);
    auto continuityPat = narrow<std::uint8_t>(f[25]);
    auto continuityPmt = narrow<std::uint8_t>(f[26]);
    auto continuityVideo = narrow<std::uint8_t>(f[27]);
    auto continuityAudio = narrow<std::uint8_t>(f[28]);
    auto maxPackets = narrow<std::uint8_t>(f[29]);
    auto timeNumerator = narrow<int>(f[21]);
    auto timeDenominator = narrow<int>(f[22]);
    auto maxAudioSamples = narrow<int>(f[31]);
    if (!tsid || !program || !pat || !pmt || !videoPid || !audioPid ||
        !pcrPid || !table || !videoType || !audioType || !nalBytes ||
        !aacMpeg || !aacObject || !aacFrequency || !aacChannels ||
        !packetSize || !continuityPat || !continuityPmt || !continuityVideo ||
        !continuityAudio || !maxPackets || !timeNumerator ||
        !timeDenominator || !maxAudioSamples ||
        f[8] > std::uint64_t{INT64_MAX} ||
        f[18] > std::uint64_t{INT64_MAX} ||
        f[19] > std::uint64_t{INT64_MAX} ||
        f[20] > std::uint64_t{INT64_MAX} ||
        f[23] > std::uint64_t{INT64_MAX} ||
        f[11] > 1 || f[13] > 1 ||
        f[30] > static_cast<unsigned>(
                    MediaOutputTransportKind::RtpAvp)) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux plan fields are out of range"));
    }
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        tsid.value(), program.value(), pat.value(), pmt.value(),
        videoPid.value(), audioPid.value(), pcrPid.value(), table.value(),
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[8])),
        videoType.value(), audioType.value(),
        static_cast<MediaTsH264InputLayout>(f[11]), nalBytes.value(),
        static_cast<MediaTsParameterSetPolicy>(f[13]),
        MediaTsAacAdtsPlan{aacMpeg.value(), aacObject.value(),
                           aacFrequency.value(), aacChannels.value()},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[18])),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[19])),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[20])),
            timeNumerator.value(), timeDenominator.value()},
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[23])),
        packetSize.value(),
        MediaTsContinuitySeeds{continuityPat.value(), continuityPmt.value(),
                               continuityVideo.value(),
                               continuityAudio.value()},
        maxPackets.value(),
        static_cast<MediaOutputTransportKind>(f[30]),
        maxAudioSamples.value()});
}

const char* familyName(MediaIpAddressFamily family) noexcept
{
    switch (family) {
    case MediaIpAddressFamily::Ipv4:
        return "ipv4";
    case MediaIpAddressFamily::Ipv6:
        return "ipv6";
    }
    return "";
}

::media::Result<MediaIpAddressFamily> parseFamily(
    const MediaNodeOptions& options,
    const char* key)
{
    auto text = requiredNodeOption(&options, Owner, key);
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
            "Project MPEG-TS RTP plan has an unknown address family"));
}

template <std::size_t Count>
bool exactOptionKeys(
    const MediaNodeOptions& options,
    const std::array<const char*, Count>& expected)
{
    if (options.values().size() != expected.size()) return false;
    for (const char* key : expected) {
        if (!options.has(key)) return false;
    }
    return true;
}

::media::Status setOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const std::vector<std::pair<std::string, std::string>>& values)
{
    for (const auto& [key, value] : values) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, nodeId, key, value);
        if (!set) return ::media::Status::failure(set.error());
    }
    return ::media::Status::success();
}

bool sameLocalPortPolicy(
    const MediaRtpUdpLocalPortPolicy& left,
    const MediaRtpUdpLocalPortPolicy& right) noexcept
{
    return left.kind() == right.kind() &&
        left.rtpPort() == right.rtpPort() &&
        left.rtcpPort() == right.rtcpPort();
}

bool sameSenderConfig(
    const MediaRtpUdpSenderConfig& left,
    const MediaRtpUdpSenderConfig& right) noexcept
{
    return left.addressFamily() == right.addressFamily() &&
        left.localNumericAddress() == right.localNumericAddress() &&
        left.remoteRtpEndpoint() == right.remoteRtpEndpoint() &&
        left.remoteRtcpEndpoint() == right.remoteRtcpEndpoint() &&
        sameLocalPortPolicy(
            left.localPortPolicy(), right.localPortPolicy()) &&
        left.sendBufferBytes() == right.sendBufferBytes() &&
        left.maximumDatagramBytes() == right.maximumDatagramBytes() &&
        left.ioBehavior() == right.ioBehavior();
}

bool sameRtpOutput(
    const MediaMpegTsRtpOutputPlan& left,
    const MediaMpegTsRtpOutputPlan& right) noexcept
{
    const auto& leftSdp = left.sdp();
    const auto& rightSdp = right.sdp();
    return sameSenderConfig(left.transport(), right.transport()) &&
        left.payloadType() == right.payloadType() &&
        left.clockRate() == right.clockRate() &&
        left.ssrc() == right.ssrc() &&
        left.baseTimestamp() == right.baseTimestamp() &&
        left.initialSequenceNumber() == right.initialSequenceNumber() &&
        left.cname() == right.cname() &&
        left.senderReportInterval() == right.senderReportInterval() &&
        left.maximumDatagramBytes() == right.maximumDatagramBytes() &&
        left.tsPacketsPerPayload() == right.tsPacketsPerPayload() &&
        leftSdp.path == rightSdp.path &&
        leftSdp.originUsername == rightSdp.originUsername &&
        leftSdp.sessionName == rightSdp.sessionName &&
        leftSdp.originAddressFamily == rightSdp.originAddressFamily &&
        leftSdp.originNumericAddress == rightSdp.originNumericAddress &&
        leftSdp.cname == rightSdp.cname;
}

bool sameProtocol(
    const MediaProjectMpegTsOutputPlan& left,
    const MediaProjectMpegTsOutputPlan& right) noexcept
{
    return left.audioSampleRate() == right.audioSampleRate() &&
        left.muxPlan().parameters() == right.muxPlan().parameters();
}

::media::Status applyUdp(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaAvSyncGroupKey& groupKey,
    const MediaProjectMpegTsRuntimeOutputPlan& output,
    const MediaMpegTsUdpOutputPlan& udp)
{
    auto endpoint = parseRtpUdpUrlEndpoint(udp.url);
    if (output.protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::UdpDatagrams ||
        !endpoint || endpoint.value().scheme != "udp" ||
        udp.resourceKind != MediaOutputResourceKind::ByteSink ||
        udp.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS UDP node plan is inconsistent"));
    }
    return setOptions(graph, nodeId, {
        {GroupKey, groupKey.value()},
        {PlanKey, encodeMux(output.protocol.muxPlan())},
        {AudioSampleRateKey,
            std::to_string(output.protocol.audioSampleRate())},
        {VariantKey, "udp"},
        {UdpKeys[4], udp.url},
        {UdpKeys[5], "byte_sink"},
        {UdpKeys[6], "project_mpegts"},
        {UdpKeys[7], "project_mpegts"}});
}

::media::Status applyRtp(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaAvSyncGroupKey& groupKey,
    const MediaProjectMpegTsRuntimeOutputPlan& output,
    const MediaMpegTsRtpOutputPlan& rtp)
{
    const auto& mux = output.protocol.muxPlan().parameters();
    const auto& sender = rtp.transport();
    const auto& localPolicy = sender.localPortPolicy();
    const auto& remoteRtp = sender.remoteRtpEndpoint();
    const auto& remoteRtcp = sender.remoteRtcpEndpoint();
    auto expectedPackets = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
        sender.maximumDatagramBytes());
    if (mux.transportKind != MediaOutputTransportKind::RtpAvp ||
        !expectedPackets ||
        mux.maximumPacketsPerDatagram != expectedPackets.value() ||
        rtp.tsPacketsPerPayload() != expectedPackets.value() ||
        localPolicy.kind() !=
            MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent ||
        localPolicy.rtpPort() || localPolicy.rtcpPort() ||
        sender.ioBehavior() !=
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP node plan is inconsistent"));
    }
    return setOptions(graph, nodeId, {
        {GroupKey, groupKey.value()},
        {PlanKey, encodeMux(output.protocol.muxPlan())},
        {AudioSampleRateKey,
            std::to_string(output.protocol.audioSampleRate())},
        {VariantKey, "rtp"},
        {RtpKeys[4], familyName(sender.addressFamily())},
        {RtpKeys[5], sender.localNumericAddress()},
        {RtpKeys[6], remoteRtp.numericAddress()},
        {RtpKeys[7], remoteRtcp.numericAddress()},
        {RtpKeys[8], std::to_string(remoteRtp.port())},
        {RtpKeys[9], std::to_string(remoteRtcp.port())},
        {RtpKeys[10], "os_assigned_independent"},
        {RtpKeys[11], "0"},
        {RtpKeys[12], "0"},
        {RtpKeys[13], std::to_string(sender.sendBufferBytes())},
        {RtpKeys[14], std::to_string(sender.maximumDatagramBytes())},
        {RtpKeys[15], "nonblocking_reject_on_pressure"},
        {RtpKeys[16], std::to_string(rtp.payloadType())},
        {RtpKeys[17], std::to_string(rtp.clockRate())},
        {RtpKeys[18], std::to_string(rtp.ssrc())},
        {RtpKeys[19], std::to_string(rtp.baseTimestamp())},
        {RtpKeys[20], rtp.cname()},
        {RtpKeys[21],
            std::to_string(rtp.senderReportInterval().nanoseconds())},
        {RtpKeys[22], std::to_string(rtp.tsPacketsPerPayload())},
        {RtpKeys[23], rtp.sdp().path},
        {RtpKeys[24], rtp.sdp().originUsername},
        {RtpKeys[25], rtp.sdp().sessionName},
        {RtpKeys[26], familyName(rtp.sdp().originAddressFamily)},
        {RtpKeys[27], rtp.sdp().originNumericAddress},
        {RtpKeys[28], rtp.sdp().cname},
        {RtpKeys[29], std::to_string(rtp.initialSequenceNumber())},
        {RtpKeys[30], "project_mpegts"}});
}

::media::Result<MediaProjectMpegTsRuntimeOutputPlan> decodeUdp(
    const MediaNode& node,
    MediaProjectMpegTsOutputPlan protocol)
{
    using Result =
        ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>;
    if (!exactOptionKeys(node.options, UdpKeys) ||
        protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::UdpDatagrams) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS UDP options have missing, extra, or mismatched fields"));
    }
    auto url = requiredNodeOption(
        &node.options, Owner, UdpKeys[4]);
    auto resource = requiredNodeOption(
        &node.options, Owner, UdpKeys[5]);
    auto muxSession = requiredNodeOption(
        &node.options, Owner, UdpKeys[6]);
    if (!url || !resource || !muxSession) {
        return Result::failure(
            !url ? url.error() :
            !resource ? resource.error() : muxSession.error());
    }
    auto endpoint = parseRtpUdpUrlEndpoint(url.value());
    if (!endpoint || endpoint.value().scheme != "udp" ||
        resource.value() != "byte_sink" ||
        muxSession.value() != "project_mpegts") {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS UDP options contain invalid transport facts"));
    }
    return Result::success(MediaProjectMpegTsRuntimeOutputPlan{
        std::move(protocol),
        MediaMuxSessionKind::ProjectMpegTs,
        std::variant<MediaMpegTsUdpOutputPlan, MediaMpegTsRtpOutputPlan>(
            std::in_place_type<MediaMpegTsUdpOutputPlan>,
            MediaMpegTsUdpOutputPlan{
                url.value(), MediaOutputResourceKind::ByteSink,
                MediaMuxSessionKind::ProjectMpegTs})});
}

::media::Result<MediaProjectMpegTsRuntimeOutputPlan> decodeRtp(
    const MediaNode& node,
    MediaProjectMpegTsOutputPlan protocol)
{
    using Result =
        ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>;
    if (!exactOptionKeys(node.options, RtpKeys) ||
        protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS RTP options have missing, extra, or mismatched fields"));
    }
    auto family = parseFamily(node.options, RtpKeys[4]);
    auto localAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[5]);
    auto remoteRtpAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[6]);
    auto remoteRtcpAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[7]);
    auto remoteRtpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[8]);
    auto remoteRtcpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[9]);
    auto localPolicy = requiredNodeOption(
        &node.options, Owner, RtpKeys[10]);
    auto localRtpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[11], true);
    auto localRtcpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[12], true);
    auto sendBuffer = parseUnsignedOption<int>(
        node.options, RtpKeys[13]);
    auto maximumDatagram = parseUnsignedOption<std::size_t>(
        node.options, RtpKeys[14]);
    auto ioBehavior = requiredNodeOption(
        &node.options, Owner, RtpKeys[15]);
    if (!family || !localAddress || !remoteRtpAddress ||
        !remoteRtcpAddress || !remoteRtpPort || !remoteRtcpPort ||
        !localPolicy || !localRtpPort || !localRtcpPort ||
        !sendBuffer || !maximumDatagram || !ioBehavior) {
        const ::media::ErrorInfo error =
            !family ? family.error() :
            !localAddress ? localAddress.error() :
            !remoteRtpAddress ? remoteRtpAddress.error() :
            !remoteRtcpAddress ? remoteRtcpAddress.error() :
            !remoteRtpPort ? remoteRtpPort.error() :
            !remoteRtcpPort ? remoteRtcpPort.error() :
            !localPolicy ? localPolicy.error() :
            !localRtpPort ? localRtpPort.error() :
            !localRtcpPort ? localRtcpPort.error() :
            !sendBuffer ? sendBuffer.error() :
            !maximumDatagram ? maximumDatagram.error() :
            ioBehavior.error();
        return Result::failure(error);
    }
    if (remoteRtpAddress.value() != remoteRtcpAddress.value() ||
        localPolicy.value() != "os_assigned_independent" ||
        localRtpPort.value() != 0 || localRtcpPort.value() != 0 ||
        ioBehavior.value() != "nonblocking_reject_on_pressure") {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS RTP options contradict the transport policy"));
    }
    auto transport = MediaRtpUdpSenderConfig::create(
        family.value(), localAddress.value(), remoteRtpAddress.value(),
        remoteRtpPort.value(), remoteRtcpPort.value(),
        MediaRtpUdpLocalPortPolicy::osAssignedIndependent(),
        sendBuffer.value(), maximumDatagram.value(),
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
    if (!transport) return Result::failure(transport.error());

    auto payloadType = parseUnsignedOption<int>(
        node.options, RtpKeys[16], true);
    auto clockRate = parseUnsignedOption<int>(
        node.options, RtpKeys[17]);
    auto ssrc = parseUnsignedOption<std::uint32_t>(
        node.options, RtpKeys[18]);
    auto baseTimestamp = parseUnsignedOption<std::uint32_t>(
        node.options, RtpKeys[19], true);
    auto initialSequenceNumber = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[29], true);
    auto cname = requiredNodeOption(
        &node.options, Owner, RtpKeys[20]);
    auto reportInterval = requiredPositiveInt64NodeOption(
        &node.options, Owner, RtpKeys[21]);
    auto packetCount = parseUnsignedOption<std::uint8_t>(
        node.options, RtpKeys[22]);
    auto sdpPath = requiredNodeOption(
        &node.options, Owner, RtpKeys[23]);
    auto originUsername = requiredNodeOption(
        &node.options, Owner, RtpKeys[24]);
    auto sessionName = requiredNodeOption(
        &node.options, Owner, RtpKeys[25]);
    auto originFamily = parseFamily(node.options, RtpKeys[26]);
    auto originAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[27]);
    auto sdpCname = requiredNodeOption(
        &node.options, Owner, RtpKeys[28]);
    if (!payloadType || !clockRate || !ssrc || !baseTimestamp ||
        !initialSequenceNumber ||
        !cname || !reportInterval || !packetCount || !sdpPath ||
        !originUsername || !sessionName || !originFamily ||
        !originAddress || !sdpCname) {
        const ::media::ErrorInfo error =
            !payloadType ? payloadType.error() :
            !clockRate ? clockRate.error() :
            !ssrc ? ssrc.error() :
            !baseTimestamp ? baseTimestamp.error() :
            !initialSequenceNumber ? initialSequenceNumber.error() :
            !cname ? cname.error() :
            !reportInterval ? reportInterval.error() :
            !packetCount ? packetCount.error() :
            !sdpPath ? sdpPath.error() :
            !originUsername ? originUsername.error() :
            !sessionName ? sessionName.error() :
            !originFamily ? originFamily.error() :
            !originAddress ? originAddress.error() :
            sdpCname.error();
        return Result::failure(error);
    }
    auto rtp = MediaMpegTsRtpOutputPlan::create(
        std::move(transport).value(), sdpPath.value(),
        originUsername.value(),
        MediaRunningTime::fromNanoseconds(reportInterval.value()));
    auto expectedPackets = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
        maximumDatagram.value());
    if (!rtp || !expectedPackets ||
        payloadType.value() != rtp.value().payloadType() ||
        clockRate.value() != rtp.value().clockRate() ||
        ssrc.value() != rtp.value().ssrc() ||
        baseTimestamp.value() != rtp.value().baseTimestamp() ||
        initialSequenceNumber.value() !=
            rtp.value().initialSequenceNumber() ||
        cname.value() != rtp.value().cname() ||
        packetCount.value() != rtp.value().tsPacketsPerPayload() ||
        packetCount.value() != expectedPackets.value() ||
        packetCount.value() !=
            protocol.muxPlan().parameters().maximumPacketsPerDatagram ||
        sessionName.value() != rtp.value().sdp().sessionName ||
        originFamily.value() != rtp.value().sdp().originAddressFamily ||
        originAddress.value() != rtp.value().sdp().originNumericAddress ||
        sdpCname.value() != rtp.value().sdp().cname) {
        return Result::failure(
            rtp ? (expectedPackets
                       ? ::media::ErrorInfo::invalidArgument(
                             "Project MPEG-TS RTP options contradict the typed plan")
                       : expectedPackets.error())
                : rtp.error());
    }
    return Result::success(MediaProjectMpegTsRuntimeOutputPlan{
        std::move(protocol),
        MediaMuxSessionKind::ProjectMpegTs,
        std::variant<MediaMpegTsUdpOutputPlan, MediaMpegTsRtpOutputPlan>(
            std::in_place_type<MediaMpegTsRtpOutputPlan>,
            std::move(rtp).value())});
}

} // namespace

::media::Status MediaProjectMpegTsPlanSourceNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaAvSyncGroupKey& groupKey,
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan)
{
    const MediaNode* node = graph.findNode(nodeId);
    if (!groupKey.valid() || !node ||
        node->kind != MediaNodeKind::ProjectMpegTsPlanSource ||
        outputPlan.protocol.audioSampleRate() <= 0 ||
        !node->options.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS plan codec requires one empty plan-source node and complete plan"));
    }
    if (const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(
                &outputPlan.transport)) {
        return applyUdp(graph, nodeId, groupKey, outputPlan, *udp);
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(&outputPlan.transport);
    return rtp
        ? applyRtp(graph, nodeId, groupKey, outputPlan, *rtp)
        : ::media::Status::failure(
              ::media::ErrorInfo::invalidArgument(
                  "Project MPEG-TS plan codec rejects an unknown transport variant"));
}

::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>
MediaProjectMpegTsPlanSourceNodePlanCodec::decode(const MediaNode& node)
{
    using Result =
        ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>;
    if (node.kind != MediaNodeKind::ProjectMpegTsPlanSource) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS plan decoder requires the plan-source node kind"));
    }
    auto groupText = requiredNodeOption(
        &node.options, Owner, GroupKey);
    auto muxText = requiredNodeOption(
        &node.options, Owner, PlanKey);
    auto audioSampleRate = parseUnsignedOption<int>(
        node.options, AudioSampleRateKey);
    auto variant = requiredNodeOption(
        &node.options, Owner, VariantKey);
    auto muxSessionKind = requiredNodeOption(
        &node.options, Owner, MuxSessionKindKey);
    if (!groupText || !muxText || !audioSampleRate || !variant ||
        !muxSessionKind) {
        return Result::failure(
            !groupText ? groupText.error() :
            !muxText ? muxText.error() :
            !audioSampleRate ? audioSampleRate.error() :
            !variant ? variant.error() : muxSessionKind.error());
    }
    if (muxSessionKind.value() != "project_mpegts") {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS node plan requires its planned mux session kind"));
    }
    MediaAvSyncGroupKey group(std::move(groupText).value());
    auto mux = decodeMux(muxText.value());
    if (!group.valid() || !mux) {
        return Result::failure(
            mux ? ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS plan source has an invalid group")
                : mux.error());
    }
    auto protocol = MediaProjectMpegTsOutputPlan::fromEncodedFacts(
        audioSampleRate.value(), std::move(mux).value());
    if (!protocol) return Result::failure(protocol.error());

    ::media::Result<MediaProjectMpegTsRuntimeOutputPlan> output =
        variant.value() == "udp"
        ? decodeUdp(node, std::move(protocol).value())
        : variant.value() == "rtp"
            ? decodeRtp(node, std::move(protocol).value())
            : ::media::Result<MediaProjectMpegTsRuntimeOutputPlan>::failure(
                  ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS plan has an unknown transport discriminator"));
    if (!output) return Result::failure(output.error());
    return Result::success(
        MediaDecodedProjectMpegTsPlanSourceNodePlan{
            std::move(group), std::move(output).value()});
}

::media::Status
MediaProjectMpegTsPlanSourceNodePlanCodec::validateAgainstPlanner(
    const MediaDecodedProjectMpegTsPlanSourceNodePlan& decoded,
    const MediaAvSyncGroupKey& plannerGroup,
    const MediaProjectMpegTsRuntimeOutputPlan& plannerProduct)
{
    if (decoded.groupKey != plannerGroup ||
        !sameProtocol(
            decoded.outputPlan.protocol, plannerProduct.protocol) ||
        decoded.outputPlan.muxSessionKind !=
            plannerProduct.muxSessionKind ||
        decoded.outputPlan.transport.index() !=
            plannerProduct.transport.index()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS node plan conflicts with its planner product"));
    }
    if (const auto* decodedUdp =
            std::get_if<MediaMpegTsUdpOutputPlan>(
                &decoded.outputPlan.transport)) {
        const auto* plannerUdp =
            std::get_if<MediaMpegTsUdpOutputPlan>(
                &plannerProduct.transport);
        if (!plannerUdp ||
            decodedUdp->url != plannerUdp->url ||
            decodedUdp->resourceKind != plannerUdp->resourceKind ||
            decodedUdp->muxSessionKind != plannerUdp->muxSessionKind) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP node plan conflicts with its planner product"));
        }
        return ::media::Status::success();
    }
    const auto* decodedRtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(
            &decoded.outputPlan.transport);
    const auto* plannerRtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(
            &plannerProduct.transport);
    if (!decodedRtp || !plannerRtp ||
        !sameRtpOutput(*decodedRtp, *plannerRtp)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP node plan conflicts with its planner product"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
