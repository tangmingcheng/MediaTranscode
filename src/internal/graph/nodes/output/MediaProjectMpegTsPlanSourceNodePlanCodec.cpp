#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <array>
#include <charconv>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaProjectMpegTsPlanSourceNodePlanCodec";
constexpr const char* SessionKey = "project_mpeg_ts_plan.session";
constexpr const char* StreamSetKey = "project_mpeg_ts_plan.stream_set";
constexpr const char* PlanKey = "project_mpeg_ts_plan.parameters";
constexpr const char* VariantKey =
    "project_mpeg_ts_plan.transport.variant";
constexpr const char* MuxSessionKindKey =
    "project_mpeg_ts_plan.mux_session_kind";
constexpr const char* EmissionVideoWindowKey =
    "project_mpeg_ts_plan.emission.video_initial_service_window_ns";
constexpr const char* EmissionAudioWindowKey =
    "project_mpeg_ts_plan.emission.audio_initial_service_window_ns";
constexpr const char* ScheduledBatchMaximumBytesKey =
    "project_mpeg_ts_plan.scheduled_batch.maximum_payload_bytes";
constexpr const char* PacingExecutionKey =
    "project_mpeg_ts_plan.pacing.execution";
constexpr const char* PacingEvidenceKey =
    "project_mpeg_ts_plan.pacing.evidence";
constexpr const char* PacingDeadlinePolicyKey =
    "project_mpeg_ts_plan.pacing.deadline_policy";
constexpr std::size_t VideoOnlyMuxFieldCount = 27;
constexpr std::size_t AudioVideoMuxFieldCount = 35;

constexpr std::array<const char*, 11> UdpKeys{
    SessionKey,
    PlanKey,
    VariantKey,
    "project_mpeg_ts_plan.transport.udp.url",
    "project_mpeg_ts_plan.transport.udp.resource_kind",
    "project_mpeg_ts_plan.transport.udp.mux_session_kind",
    MuxSessionKindKey,
    StreamSetKey,
    EmissionVideoWindowKey,
    EmissionAudioWindowKey,
    ScheduledBatchMaximumBytesKey};

constexpr std::array<const char*, 37> RtpKeys{
    SessionKey,
    PlanKey,
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
    MuxSessionKindKey,
    StreamSetKey,
    EmissionVideoWindowKey,
    EmissionAudioWindowKey,
    ScheduledBatchMaximumBytesKey,
    PacingExecutionKey,
    PacingEvidenceKey,
    PacingDeadlinePolicyKey};

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
    bool allowZero)
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

::media::Result<MediaTsDatagramEmissionPlan> decodeEmission(
    const MediaNodeOptions& options,
    const MediaTsMuxPlan& muxPlan)
{
    auto videoWindow = requiredPositiveInt64NodeOption(
        &options, Owner, EmissionVideoWindowKey);
    auto audioWindow = parseUnsignedOption<std::int64_t>(
        options, EmissionAudioWindowKey, true);
    if (!videoWindow || !audioWindow) {
        return ::media::Result<MediaTsDatagramEmissionPlan>::failure(
            videoWindow ? audioWindow.error() : videoWindow.error());
    }
    auto emission = MediaTsDatagramEmissionPlan::create(
        muxPlan,
        MediaRunningTime::fromNanoseconds(videoWindow.value()),
        audioWindow.value() == 0
            ? std::nullopt
            : std::optional<MediaRunningTime>(
                  MediaRunningTime::fromNanoseconds(audioWindow.value())));
    if (!emission ||
        emission.value().videoInitialServiceWindow().nanoseconds() !=
            videoWindow.value() ||
        (audioWindow.value() == 0) !=
            !emission.value().audioInitialServiceWindow() ||
        (audioWindow.value() != 0 &&
         emission.value().audioInitialServiceWindow()->nanoseconds() !=
             audioWindow.value())) {
        return ::media::Result<MediaTsDatagramEmissionPlan>::failure(
            emission ? ::media::ErrorInfo::invalidArgument(
                           "Project MPEG-TS emission windows are not canonical")
                     : emission.error());
    }
    return emission;
}

::media::Result<std::vector<std::uint64_t>>
parseMuxFields(std::string_view text)
{
    std::vector<std::uint64_t> fields;
    fields.reserve(AudioVideoMuxFieldCount);
    while (!text.empty()) {
        const auto separator = text.find(',');
        const auto token = text.substr(0, separator);
        if (token.empty()) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS mux plan contains an empty field"));
        }
        const char* begin = token.data();
        const char* end = begin + token.size();
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(begin, end, value, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS mux plan contains a non-numeric field"));
        }
        fields.push_back(value);
        if (separator == std::string_view::npos) {
            text = {};
        } else {
            text.remove_prefix(separator + 1);
        }
    }
    if (fields.empty() ||
        (fields.front() == 0 && fields.size() != VideoOnlyMuxFieldCount) ||
        (fields.front() == 1 && fields.size() != AudioVideoMuxFieldCount) ||
        fields.front() > 1) {
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
    const bool videoOnly = muxPlan.videoOnlyProgram() != nullptr;
    encoded << (videoOnly ? 0 : 1) << ','
            << p.transportStreamId << ',' << p.programNumber << ','
            << p.patPid << ',' << p.programMapPid << ','
            << static_cast<unsigned>(p.tableVersion) << ','
            << p.psiRepeatInterval.nanoseconds() << ','
            << static_cast<unsigned>(p.video.layout()) << ','
            << static_cast<unsigned>(p.video.nalLengthBytes()) << ','
            << static_cast<unsigned>(p.parameterSetPolicy) << ','
            << p.clock.pcrInterval.nanoseconds() << ','
            << p.clock.maximumPcrGap.nanoseconds() << ','
            << p.clock.maximumPcrJitter.nanoseconds() << ','
            << p.clock.timestampTimeBaseNumerator << ','
            << p.clock.timestampTimeBaseDenominator << ','
            << p.transportDecodeLead.nanoseconds() << ','
            << p.startupEmissionPreroll.nanoseconds() << ','
            << p.packetSize << ','
            << static_cast<unsigned>(p.maximumPacketsPerDatagram) << ','
            << static_cast<unsigned>(p.transportKind) << ','
            << static_cast<unsigned>(p.video.codec());
    if (const auto* video = muxPlan.videoOnlyProgram()) {
        encoded << ',' << video->videoPid << ',' << video->pcrPid << ','
                << static_cast<unsigned>(video->videoStreamType) << ','
                << static_cast<unsigned>(video->continuity.pat) << ','
                << static_cast<unsigned>(video->continuity.pmt) << ','
                << static_cast<unsigned>(video->continuity.video);
    } else {
        const auto& av = *muxPlan.audioVideoProgram();
        encoded << ',' << av.videoPid << ',' << av.audioPid << ','
                << av.pcrPid << ','
                << static_cast<unsigned>(av.videoStreamType) << ','
                << static_cast<unsigned>(av.audioStreamType) << ','
                << static_cast<unsigned>(av.aac.mpegId) << ','
                << static_cast<unsigned>(av.aac.audioObjectType) << ','
                << static_cast<unsigned>(av.aac.samplingFrequencyIndex) << ','
                << static_cast<unsigned>(av.aac.channelConfiguration) << ','
                << static_cast<unsigned>(av.continuity.pat) << ','
                << static_cast<unsigned>(av.continuity.pmt) << ','
                << static_cast<unsigned>(av.continuity.video) << ','
                << static_cast<unsigned>(av.continuity.audio) << ','
                << av.maximumAudioAccessUnitSamples;
    }
    return encoded.str();
}

::media::Result<MediaTsMuxPlan> decodeMux(std::string_view text)
{
    auto parsed = parseMuxFields(text);
    if (!parsed) {
        return ::media::Result<MediaTsMuxPlan>::failure(parsed.error());
    }
    const auto& f = parsed.value();
    auto tsid = narrow<std::uint16_t>(f[1]);
    auto programNumber = narrow<std::uint16_t>(f[2]);
    auto pat = narrow<std::uint16_t>(f[3]);
    auto pmt = narrow<std::uint16_t>(f[4]);
    auto table = narrow<std::uint8_t>(f[5]);
    auto nalBytes = narrow<std::uint8_t>(f[8]);
    auto packetSize = narrow<std::uint16_t>(f[17]);
    auto maxPackets = narrow<std::uint8_t>(f[18]);
    auto timeNumerator = narrow<int>(f[13]);
    auto timeDenominator = narrow<int>(f[14]);
    if (!tsid || !programNumber || !pat || !pmt || !table || !nalBytes ||
        !packetSize || !maxPackets || !timeNumerator || !timeDenominator ||
        f[6] > std::uint64_t{INT64_MAX} ||
        f[10] > std::uint64_t{INT64_MAX} ||
        f[11] > std::uint64_t{INT64_MAX} ||
        f[12] > std::uint64_t{INT64_MAX} ||
        f[15] > std::uint64_t{INT64_MAX} ||
        f[16] > std::uint64_t{INT64_MAX} ||
        f[7] > static_cast<unsigned>(MediaTsNalLayout::LengthPrefixed) ||
        f[9] > 1 ||
        f[20] > static_cast<unsigned>(MediaTsVideoCodec::Hevc) ||
        f[19] > static_cast<unsigned>(
                     MediaOutputTransportKind::RtpAvp)) {
        return ::media::Result<MediaTsMuxPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS mux plan fields are out of range"));
    }
    MediaTsProgramPlan programPlan;
    if (f[0] == 0) {
        auto videoPid = narrow<std::uint16_t>(f[21]);
        auto pcrPid = narrow<std::uint16_t>(f[22]);
        auto videoType = narrow<std::uint8_t>(f[23]);
        auto continuityPat = narrow<std::uint8_t>(f[24]);
        auto continuityPmt = narrow<std::uint8_t>(f[25]);
        auto continuityVideo = narrow<std::uint8_t>(f[26]);
        if (!videoPid || !pcrPid || !videoType || !continuityPat ||
            !continuityPmt || !continuityVideo) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS VideoOnly mux fields are out of range"));
        }
        programPlan.emplace<MediaTsVideoOnlyProgramPlan>(
            MediaTsVideoOnlyProgramPlan{
                videoPid.value(), pcrPid.value(), videoType.value(),
                MediaTsVideoContinuitySeeds{
                    continuityPat.value(), continuityPmt.value(),
                    continuityVideo.value()}});
    } else {
        auto videoPid = narrow<std::uint16_t>(f[21]);
        auto audioPid = narrow<std::uint16_t>(f[22]);
        auto pcrPid = narrow<std::uint16_t>(f[23]);
        auto videoType = narrow<std::uint8_t>(f[24]);
        auto audioType = narrow<std::uint8_t>(f[25]);
        auto aacMpeg = narrow<std::uint8_t>(f[26]);
        auto aacObject = narrow<std::uint8_t>(f[27]);
        auto aacFrequency = narrow<std::uint8_t>(f[28]);
        auto aacChannels = narrow<std::uint8_t>(f[29]);
        auto continuityPat = narrow<std::uint8_t>(f[30]);
        auto continuityPmt = narrow<std::uint8_t>(f[31]);
        auto continuityVideo = narrow<std::uint8_t>(f[32]);
        auto continuityAudio = narrow<std::uint8_t>(f[33]);
        auto maxAudioSamples = narrow<int>(f[34]);
        if (!videoPid || !audioPid || !pcrPid || !videoType || !audioType ||
            !aacMpeg || !aacObject || !aacFrequency || !aacChannels ||
            !continuityPat || !continuityPmt || !continuityVideo ||
            !continuityAudio || !maxAudioSamples) {
            return ::media::Result<MediaTsMuxPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS AudioVideo mux fields are out of range"));
        }
        programPlan.emplace<MediaTsAudioVideoProgramPlan>(
            MediaTsAudioVideoProgramPlan{
                videoPid.value(), audioPid.value(), pcrPid.value(),
                videoType.value(), audioType.value(),
                MediaTsAacAdtsPlan{
                    aacMpeg.value(), aacObject.value(),
                    aacFrequency.value(), aacChannels.value()},
                MediaTsAudioVideoContinuitySeeds{
                    continuityPat.value(), continuityPmt.value(),
                    continuityVideo.value(), continuityAudio.value()},
                maxAudioSamples.value()});
    }
    const auto videoStreamType = std::visit(
        [](const auto& program) { return program.videoStreamType; },
        programPlan);
    auto video = MediaTsVideoElementaryStreamContract::create(
        static_cast<MediaTsVideoCodec>(f[20]),
        static_cast<MediaTsNalLayout>(f[7]), nalBytes.value(),
        videoStreamType);
    if (!video) {
        return ::media::Result<MediaTsMuxPlan>::failure(video.error());
    }
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        tsid.value(), programNumber.value(), pat.value(), pmt.value(),
        table.value(),
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[6])),
        std::move(programPlan), std::move(video).value(),
        static_cast<MediaTsParameterSetPolicy>(f[9]),
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[10])),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[11])),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(f[12])),
            timeNumerator.value(), timeDenominator.value()},
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[15])),
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[16])),
        packetSize.value(), maxPackets.value(),
        static_cast<MediaOutputTransportKind>(f[19])});
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
        left.pacing() == right.pacing() &&
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
    return left.muxPlan().parameters() == right.muxPlan().parameters();
}

::media::Status applyUdp(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaProtocolOutputSessionKey& sessionKey,
    MediaTranscodeStreamSet streamSet,
    const MediaProjectMpegTsRuntimeOutputPlan& output,
    const MediaMpegTsUdpOutputPlan& udp)
{
    auto endpoint = parseRtpUdpUrlEndpoint(udp.url);
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        output.protocol.muxPlan(),
        output.emission.videoInitialServiceWindow(),
        output.emission.audioInitialServiceWindow());
    if (output.protocol.muxPlan().parameters().transportKind !=
            MediaOutputTransportKind::UdpDatagrams ||
        !expectedEmission || output.emission != expectedEmission.value() ||
        !encodedStreamSet || !endpoint || endpoint.value().scheme != "udp" ||
        udp.resourceKind != MediaOutputResourceKind::ByteSink ||
        udp.muxSessionKind != MediaMuxSessionKind::ProjectMpegTs ||
        output.scheduledBatchMaximumBytes != 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS UDP node plan is inconsistent"));
    }
    return setOptions(graph, nodeId, {
        {SessionKey, sessionKey.value()},
        {PlanKey, encodeMux(output.protocol.muxPlan())},
        {VariantKey, "udp"},
        {UdpKeys[3], udp.url},
        {UdpKeys[4], "byte_sink"},
        {UdpKeys[5], "project_mpegts"},
        {UdpKeys[6], "project_mpegts"},
        {StreamSetKey, std::string(encodedStreamSet.value())},
        {EmissionVideoWindowKey, std::to_string(
             output.emission.videoInitialServiceWindow().nanoseconds())},
         {EmissionAudioWindowKey, std::to_string(
              output.emission.audioInitialServiceWindow()
                  ? output.emission.audioInitialServiceWindow()->nanoseconds()
                  : 0)},
         {ScheduledBatchMaximumBytesKey, "0"}});
}

::media::Status applyRtp(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaProtocolOutputSessionKey& sessionKey,
    MediaTranscodeStreamSet streamSet,
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
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        output.protocol.muxPlan(),
        output.emission.videoInitialServiceWindow(),
        output.emission.audioInitialServiceWindow());
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    if (mux.transportKind != MediaOutputTransportKind::RtpAvp ||
        !encodedStreamSet || !expectedPackets ||
        mux.maximumPacketsPerDatagram != expectedPackets.value() ||
        rtp.tsPacketsPerPayload() != expectedPackets.value() ||
        !expectedEmission || output.emission != expectedEmission.value() ||
        output.emission.maximumWireDatagramBytes() >
            sender.maximumDatagramBytes() ||
        rtp.pacing().execution !=
            MediaDatagramDispatchExecution::UserspaceWaitAndSend ||
        rtp.pacing().evidence !=
            MediaDatagramTimingEvidence::UserspaceSendReturn ||
        rtp.pacing().deadlinePolicy !=
            MediaDatagramDeadlinePolicy::CanonicalOrdered ||
        localPolicy.kind() !=
            MediaRtpUdpLocalPortPolicyKind::OsAssignedIndependent ||
        localPolicy.rtpPort() || localPolicy.rtcpPort() ||
        output.scheduledBatchMaximumBytes == 0 ||
        sender.ioBehavior() !=
            MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP node plan is inconsistent"));
    }
    return setOptions(graph, nodeId, {
        {SessionKey, sessionKey.value()},
        {PlanKey, encodeMux(output.protocol.muxPlan())},
        {VariantKey, "rtp"},
        {RtpKeys[3], familyName(sender.addressFamily())},
        {RtpKeys[4], sender.localNumericAddress()},
        {RtpKeys[5], remoteRtp.numericAddress()},
        {RtpKeys[6], remoteRtcp.numericAddress()},
        {RtpKeys[7], std::to_string(remoteRtp.port())},
        {RtpKeys[8], std::to_string(remoteRtcp.port())},
        {RtpKeys[9], "os_assigned_independent"},
        {RtpKeys[10], "0"},
        {RtpKeys[11], "0"},
        {RtpKeys[12], std::to_string(sender.sendBufferBytes())},
        {RtpKeys[13], std::to_string(sender.maximumDatagramBytes())},
        {RtpKeys[14], "nonblocking_reject_on_pressure"},
        {RtpKeys[15], std::to_string(rtp.payloadType())},
        {RtpKeys[16], std::to_string(rtp.clockRate())},
        {RtpKeys[17], std::to_string(rtp.ssrc())},
        {RtpKeys[18], std::to_string(rtp.baseTimestamp())},
        {RtpKeys[19], rtp.cname()},
        {RtpKeys[20],
            std::to_string(rtp.senderReportInterval().nanoseconds())},
        {RtpKeys[21], std::to_string(rtp.tsPacketsPerPayload())},
        {RtpKeys[22], rtp.sdp().path},
        {RtpKeys[23], rtp.sdp().originUsername},
        {RtpKeys[24], rtp.sdp().sessionName},
        {RtpKeys[25], familyName(rtp.sdp().originAddressFamily)},
        {RtpKeys[26], rtp.sdp().originNumericAddress},
        {RtpKeys[27], rtp.sdp().cname},
        {RtpKeys[28], std::to_string(rtp.initialSequenceNumber())},
        {RtpKeys[29], "project_mpegts"},
        {StreamSetKey, std::string(encodedStreamSet.value())},
        {EmissionVideoWindowKey, std::to_string(
             output.emission.videoInitialServiceWindow().nanoseconds())},
         {EmissionAudioWindowKey, std::to_string(
              output.emission.audioInitialServiceWindow()
                  ? output.emission.audioInitialServiceWindow()->nanoseconds()
                  : 0)},
         {ScheduledBatchMaximumBytesKey,
          std::to_string(output.scheduledBatchMaximumBytes)},
         {PacingExecutionKey, "userspace_wait_and_send"},
         {PacingEvidenceKey, "userspace_send_return"},
         {PacingDeadlinePolicyKey, "canonical_ordered"}});
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
        &node.options, Owner, UdpKeys[3]);
    auto resource = requiredNodeOption(
        &node.options, Owner, UdpKeys[4]);
    auto muxSession = requiredNodeOption(
        &node.options, Owner, UdpKeys[5]);
    auto scheduledBatchMaximumBytes = parseUnsignedOption<std::uint64_t>(
        node.options, ScheduledBatchMaximumBytesKey, true);
    if (!url || !resource || !muxSession || !scheduledBatchMaximumBytes) {
        return Result::failure(
            !url ? url.error() :
            !resource ? resource.error() :
            !muxSession ? muxSession.error() :
            scheduledBatchMaximumBytes.error());
    }
    auto endpoint = parseRtpUdpUrlEndpoint(url.value());
    auto emission = decodeEmission(node.options, protocol.muxPlan());
    if (!endpoint || endpoint.value().scheme != "udp" ||
        !emission ||
        resource.value() != "byte_sink" ||
        muxSession.value() != "project_mpegts" ||
        scheduledBatchMaximumBytes.value() != 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS UDP options contain invalid transport facts"));
    }
    return Result::success(MediaProjectMpegTsRuntimeOutputPlan{
        std::move(protocol),
        MediaMuxSessionKind::ProjectMpegTs,
        std::move(emission).value(),
        0,
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
    auto family = parseFamily(node.options, RtpKeys[3]);
    auto localAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[4]);
    auto remoteRtpAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[5]);
    auto remoteRtcpAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[6]);
    auto remoteRtpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[7], false);
    auto remoteRtcpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[8], false);
    auto localPolicy = requiredNodeOption(
        &node.options, Owner, RtpKeys[9]);
    auto localRtpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[10], true);
    auto localRtcpPort = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[11], true);
    auto sendBuffer = parseUnsignedOption<int>(
        node.options, RtpKeys[12], false);
    auto maximumDatagram = parseUnsignedOption<std::size_t>(
        node.options, RtpKeys[13], false);
    auto ioBehavior = requiredNodeOption(
        &node.options, Owner, RtpKeys[14]);
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
        node.options, RtpKeys[15], true);
    auto clockRate = parseUnsignedOption<int>(
        node.options, RtpKeys[16], false);
    auto ssrc = parseUnsignedOption<std::uint32_t>(
        node.options, RtpKeys[17], false);
    auto baseTimestamp = parseUnsignedOption<std::uint32_t>(
        node.options, RtpKeys[18], true);
    auto initialSequenceNumber = parseUnsignedOption<std::uint16_t>(
        node.options, RtpKeys[28], true);
    auto cname = requiredNodeOption(
        &node.options, Owner, RtpKeys[19]);
    auto reportInterval = requiredPositiveInt64NodeOption(
        &node.options, Owner, RtpKeys[20]);
    auto packetCount = parseUnsignedOption<std::uint8_t>(
        node.options, RtpKeys[21], false);
    auto sdpPath = requiredNodeOption(
        &node.options, Owner, RtpKeys[22]);
    auto originUsername = requiredNodeOption(
        &node.options, Owner, RtpKeys[23]);
    auto sessionName = requiredNodeOption(
        &node.options, Owner, RtpKeys[24]);
    auto originFamily = parseFamily(node.options, RtpKeys[25]);
    auto originAddress = requiredNodeOption(
        &node.options, Owner, RtpKeys[26]);
    auto sdpCname = requiredNodeOption(
        &node.options, Owner, RtpKeys[27]);
    auto pacingExecution = requiredNodeOption(
        &node.options, Owner, PacingExecutionKey);
    auto pacingEvidence = requiredNodeOption(
        &node.options, Owner, PacingEvidenceKey);
    auto pacingDeadlinePolicy = requiredNodeOption(
        &node.options, Owner, PacingDeadlinePolicyKey);
    if (!payloadType || !clockRate || !ssrc || !baseTimestamp ||
        !initialSequenceNumber ||
        !cname || !reportInterval || !packetCount || !sdpPath ||
        !originUsername || !sessionName || !originFamily ||
        !originAddress || !sdpCname || !pacingExecution ||
        !pacingEvidence || !pacingDeadlinePolicy) {
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
            !sdpCname ? sdpCname.error() :
            !pacingExecution ? pacingExecution.error() :
            !pacingEvidence ? pacingEvidence.error() :
            pacingDeadlinePolicy.error();
        return Result::failure(error);
    }
    if (pacingExecution.value() != "userspace_wait_and_send" ||
        pacingEvidence.value() != "userspace_send_return" ||
        pacingDeadlinePolicy.value() != "canonical_ordered") {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS RTP pacing capability is unsupported"));
    }
    const MediaScheduledDatagramPacingPlan pacing{
        MediaDatagramDispatchExecution::UserspaceWaitAndSend,
        MediaDatagramTimingEvidence::UserspaceSendReturn,
        MediaDatagramDeadlinePolicy::CanonicalOrdered};
    auto rtp = MediaMpegTsRtpOutputPlan::create(
        std::move(transport).value(), sdpPath.value(),
        originUsername.value(),
        MediaRunningTime::fromNanoseconds(reportInterval.value()),
        pacing);
    auto expectedPackets = MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
        maximumDatagram.value());
    auto emission = decodeEmission(node.options, protocol.muxPlan());
    auto scheduledBatchMaximumBytes = parseUnsignedOption<std::uint64_t>(
        node.options, ScheduledBatchMaximumBytesKey, false);
    if (!rtp || !expectedPackets ||
        !emission ||
        !scheduledBatchMaximumBytes ||
        emission.value().maximumWireDatagramBytes() >
            maximumDatagram.value() ||
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
        std::move(emission).value(),
        scheduledBatchMaximumBytes.value(),
        std::variant<MediaMpegTsUdpOutputPlan, MediaMpegTsRtpOutputPlan>(
            std::in_place_type<MediaMpegTsRtpOutputPlan>,
            std::move(rtp).value())});
}

} // namespace

::media::Status MediaProjectMpegTsPlanSourceNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaProtocolOutputSessionKey& sessionKey,
    MediaTranscodeStreamSet streamSet,
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan)
{
    const MediaNode* node = graph.findNode(nodeId);
    auto encodedStreamSet = MediaTranscodeStreamSetCodec::encode(streamSet);
    const bool typedVideoOnly =
        outputPlan.protocol.muxPlan().videoOnlyProgram() != nullptr;
    if (!sessionKey.valid() || !node || !encodedStreamSet ||
        node->kind != MediaNodeKind::ProjectMpegTsPlanSource ||
        (streamSet == MediaTranscodeStreamSet::VideoOnly) != typedVideoOnly ||
        !node->options.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS plan codec requires one empty plan-source node and complete plan"));
    }
    if (const auto* udp =
            std::get_if<MediaMpegTsUdpOutputPlan>(
                &outputPlan.transport)) {
        return applyUdp(
            graph, nodeId, sessionKey, streamSet, outputPlan, *udp);
    }
    const auto* rtp =
        std::get_if<MediaMpegTsRtpOutputPlan>(&outputPlan.transport);
    return rtp
        ? applyRtp(
              graph, nodeId, sessionKey, streamSet, outputPlan, *rtp)
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
    auto sessionText = requiredNodeOption(
        &node.options, Owner, SessionKey);
    auto streamSetText = requiredNodeOption(
        &node.options, Owner, StreamSetKey);
    auto muxText = requiredNodeOption(
        &node.options, Owner, PlanKey);
    auto variant = requiredNodeOption(
        &node.options, Owner, VariantKey);
    auto muxSessionKind = requiredNodeOption(
        &node.options, Owner, MuxSessionKindKey);
    if (!sessionText || !streamSetText || !muxText || !variant ||
        !muxSessionKind) {
        return Result::failure(
            !sessionText ? sessionText.error() :
            !streamSetText ? streamSetText.error() :
            !muxText ? muxText.error() :
            !variant ? variant.error() : muxSessionKind.error());
    }
    if (muxSessionKind.value() != "project_mpegts") {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS node plan requires its planned mux session kind"));
    }
    MediaProtocolOutputSessionKey session(std::move(sessionText).value());
    auto streamSet = MediaTranscodeStreamSetCodec::decode(
        streamSetText.value());
    if (!streamSet) return Result::failure(streamSet.error());
    auto mux = decodeMux(muxText.value());
    if (!session.valid() || !mux) {
        return Result::failure(
            mux ? ::media::ErrorInfo::invalidArgument(
                      "Project MPEG-TS plan source has an invalid session")
                : mux.error());
    }
    const bool typedVideoOnly = mux.value().videoOnlyProgram() != nullptr;
    if ((streamSet.value() == MediaTranscodeStreamSet::VideoOnly) !=
        typedVideoOnly) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS stream set conflicts with its typed program"));
    }
    auto protocol = streamSet.value() == MediaTranscodeStreamSet::VideoOnly
        ? MediaProjectMpegTsOutputPlan::fromVideoOnlyEncodedFacts(
              std::move(mux).value())
        : MediaProjectMpegTsOutputPlan::fromAudioVideoEncodedFacts(
              std::move(mux).value());
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
            std::move(session), std::move(streamSet).value(),
            std::move(output).value()});
}

::media::Status
MediaProjectMpegTsPlanSourceNodePlanCodec::validateAgainstPlanner(
    const MediaDecodedProjectMpegTsPlanSourceNodePlan& decoded,
    const MediaProtocolOutputSessionKey& plannerSession,
    MediaTranscodeStreamSet plannerStreamSet,
    const MediaProjectMpegTsRuntimeOutputPlan& plannerProduct)
{
    if (decoded.sessionKey != plannerSession ||
        decoded.streamSet != plannerStreamSet ||
        !sameProtocol(
            decoded.outputPlan.protocol, plannerProduct.protocol) ||
        decoded.outputPlan.muxSessionKind !=
            plannerProduct.muxSessionKind ||
        decoded.outputPlan.emission != plannerProduct.emission ||
        decoded.outputPlan.scheduledBatchMaximumBytes !=
            plannerProduct.scheduledBatchMaximumBytes ||
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
