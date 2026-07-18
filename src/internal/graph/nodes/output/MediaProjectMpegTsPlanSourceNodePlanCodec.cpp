#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <array>
#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* Owner =
    "MediaProjectMpegTsPlanSourceNodePlanCodec";
constexpr const char* GroupKey = "project_mpeg_ts_plan.sync_group";
constexpr const char* PlanKey = "project_mpeg_ts_plan.parameters";
constexpr std::size_t FieldCount = 32;

template <typename Value>
::media::Result<Value> narrow(std::uint64_t value)
{
    if (value > static_cast<std::uint64_t>((std::numeric_limits<Value>::max)())) {
        return ::media::Result<Value>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS plan option contains an out-of-range field"));
    }
    return ::media::Result<Value>::success(static_cast<Value>(value));
}

::media::Result<std::array<std::uint64_t, FieldCount>> parseFields(
    std::string_view text)
{
    std::array<std::uint64_t, FieldCount> fields{};
    std::size_t count = 0;
    while (!text.empty() && count < fields.size()) {
        const auto separator = text.find(',');
        const auto token = text.substr(0, separator);
        if (token.empty()) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan option contains an empty field"));
        }
        const char* begin = token.data();
        const char* end = begin + token.size();
        const auto parsed = std::from_chars(begin, end, fields[count], 10);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            return ::media::Result<decltype(fields)>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan option contains a non-numeric field"));
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
                "Project MPEG-TS plan option has the wrong field count"));
    }
    return ::media::Result<decltype(fields)>::success(fields);
}

} // namespace

::media::Status MediaProjectMpegTsPlanSourceNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaAvSyncGroupKey& groupKey,
    const MediaTsMuxPlan& muxPlan)
{
    if (!groupKey.valid() || !graph.findNode(nodeId)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS plan source codec requires a node and group"));
    }
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
    for (const auto& [key, value] : {
             std::pair<std::string, std::string>{GroupKey, groupKey.value()},
             {PlanKey, encoded.str()}}) {
        auto set = MediaGraphBuildSupport::setNodeOptionChecked(
            graph, Owner, nodeId, key, value);
        if (!set) return ::media::Status::failure(set.error());
    }
    return ::media::Status::success();
}

::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>
MediaProjectMpegTsPlanSourceNodePlanCodec::decode(const MediaNode& node)
{
    auto groupText = requiredNodeOption(
        &node.options, Owner, GroupKey);
    auto planText = requiredNodeOption(&node.options, Owner, PlanKey);
    if (!groupText || !planText) {
        return ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>::failure(
            groupText ? planText.error() : groupText.error());
    }
    MediaAvSyncGroupKey group(std::move(groupText).value());
    auto parsed = parseFields(planText.value());
    if (!group.valid() || !parsed) {
        return ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>::failure(
            parsed ? ::media::ErrorInfo::invalidArgument(
                         "Project MPEG-TS plan source has an invalid group")
                   : parsed.error());
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
        f[8] > std::uint64_t{INT64_MAX} || f[18] > std::uint64_t{INT64_MAX} ||
        f[19] > std::uint64_t{INT64_MAX} || f[20] > std::uint64_t{INT64_MAX} ||
        f[23] > std::uint64_t{INT64_MAX} || f[11] > 1 || f[13] > 1 ||
        f[30] != static_cast<unsigned>(MediaTsOutputTransportKind::Udp)) {
        return ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS plan source options are out of range"));
    }
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        tsid.value(), program.value(), pat.value(), pmt.value(),
        videoPid.value(), audioPid.value(), pcrPid.value(), table.value(),
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[8])),
        videoType.value(), audioType.value(),
        static_cast<MediaTsH264InputLayout>(f[11]), nalBytes.value(),
        static_cast<MediaTsParameterSetPolicy>(f[13]),
        MediaTsAacAdtsPlan{aacMpeg.value(), aacObject.value(),
                           aacFrequency.value(), aacChannels.value()},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[18])),
            MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[19])),
            MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[20])),
            timeNumerator.value(), timeDenominator.value()},
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(f[23])),
        packetSize.value(),
        MediaTsContinuitySeeds{continuityPat.value(), continuityPmt.value(),
                               continuityVideo.value(), continuityAudio.value()},
        maxPackets.value(), MediaTsOutputTransportKind::Udp,
        maxAudioSamples.value()});
    if (!mux) {
        return ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>::failure(
            mux.error());
    }
    return ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan>::success(
        {std::move(group), std::move(mux).value()});
}

} // namespace media::ffmpeg::graph
