#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner =
    "MediaDemuxPacketClockBinderNodePlanCodec";
constexpr const char* NodeName = "MediaDemuxPacketClockBinderNode";

template <typename Integer>
::media::Result<Integer> parseInteger(
    const MediaNodeOptions& options,
    const char* key,
    bool requirePositive)
{
    auto text = requiredNodeOption(&options, NodeName, key);
    if (!text) return ::media::Result<Integer>::failure(text.error());
    Integer value = 0;
    const char* begin = text.value().data();
    const char* end = begin + text.value().size();
    const auto parsed = std::from_chars(begin, end, value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        (requirePositive && value <= 0)) {
        return ::media::Result<Integer>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(NodeName) + " requires exact integer option: " +
                key));
    }
    return ::media::Result<Integer>::success(value);
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

::media::Result<MediaScheduledStream> parseStream(
    const MediaNodeOptions& options)
{
    auto stream = requiredNodeOption(
        &options, NodeName, "demux_clock_binder.stream");
    if (!stream) {
        return ::media::Result<MediaScheduledStream>::failure(
            stream.error());
    }
    if (stream.value() == "video") {
        return ::media::Result<MediaScheduledStream>::success(
            MediaScheduledStream::Video);
    }
    if (stream.value() == "audio") {
        return ::media::Result<MediaScheduledStream>::success(
            MediaScheduledStream::Audio);
    }
    return ::media::Result<MediaScheduledStream>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Demux packet clock binder requires an explicit stream"));
}

} // namespace

::media::Status MediaDemuxPacketClockBinderNodePlanCodec::apply(
    MediaGraph& graph,
    MediaNodeId nodeId,
    MediaScheduledStream stream,
    const MediaAvSyncGroupKey& groupKey,
    const MediaDemuxTimestampInputClockAssemblyPlan& plan)
{
    const MediaRational& streamTimeBase =
        stream == MediaScheduledStream::Video
        ? plan.videoTimeBase
        : plan.audioTimeBase;
    MediaDemuxTimestampClockMapperConfig config{
        plan.videoTimeBase,
        plan.audioTimeBase,
        plan.firstWindowMaximumSkew,
        plan.timestampRegressionLimit,
        plan.discontinuityThreshold,
        plan.initialGeneration,
        plan.videoSourceIdentity,
        plan.audioSourceIdentity,
        plan.canonicalTargetEpoch};
    if (!graph.findNode(nodeId) || !groupKey.valid() ||
        streamTimeBase.num <= 0 || streamTimeBase.den <= 0 ||
        !MediaDemuxTimestampClockMapper::create(config)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux binder node options require one complete planner product"));
    }
    return setOptions(graph, nodeId, {
        {"demux_clock_binder.stream",
             stream == MediaScheduledStream::Video ? "video" : "audio"},
        {"demux_clock_binder.sync_group", groupKey.value()},
        {"demux_clock_binder.time_base_num",
             std::to_string(streamTimeBase.num)},
        {"demux_clock_binder.time_base_den",
             std::to_string(streamTimeBase.den)},
        {"demux_clock_binder.video_time_base_num",
             std::to_string(plan.videoTimeBase.num)},
        {"demux_clock_binder.video_time_base_den",
             std::to_string(plan.videoTimeBase.den)},
        {"demux_clock_binder.audio_time_base_num",
             std::to_string(plan.audioTimeBase.num)},
        {"demux_clock_binder.audio_time_base_den",
             std::to_string(plan.audioTimeBase.den)},
        {"demux_clock_binder.first_window_maximum_skew_ns",
             std::to_string(plan.firstWindowMaximumSkew.nanoseconds())},
        {"demux_clock_binder.timestamp_regression_limit_ns",
             std::to_string(plan.timestampRegressionLimit.nanoseconds())},
        {"demux_clock_binder.discontinuity_threshold_ns",
             std::to_string(plan.discontinuityThreshold.nanoseconds())},
        {"demux_clock_binder.initial_generation",
             std::to_string(plan.initialGeneration)},
        {"demux_clock_binder.video_source_identity",
             plan.videoSourceIdentity},
        {"demux_clock_binder.audio_source_identity",
             plan.audioSourceIdentity},
        {"demux_clock_binder.canonical_target_epoch_ns",
             std::to_string(plan.canonicalTargetEpoch.nanoseconds())}
    });
}

::media::Result<MediaDecodedDemuxPacketClockBinderNodePlan>
MediaDemuxPacketClockBinderNodePlanCodec::decode(const MediaNode& node)
{
    using Result =
        ::media::Result<MediaDecodedDemuxPacketClockBinderNodePlan>;
    if (node.kind != MediaNodeKind::DemuxPacketClockBinder) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Demux binder plan decoder requires the binder node kind"));
    }
    auto stream = parseStream(node.options);
    auto groupText = requiredNodeOption(
        &node.options, NodeName, "demux_clock_binder.sync_group");
    auto streamNum = parseInteger<int>(
        node.options, "demux_clock_binder.time_base_num", true);
    auto streamDen = parseInteger<int>(
        node.options, "demux_clock_binder.time_base_den", true);
    auto videoNum = parseInteger<int>(
        node.options, "demux_clock_binder.video_time_base_num", true);
    auto videoDen = parseInteger<int>(
        node.options, "demux_clock_binder.video_time_base_den", true);
    auto audioNum = parseInteger<int>(
        node.options, "demux_clock_binder.audio_time_base_num", true);
    auto audioDen = parseInteger<int>(
        node.options, "demux_clock_binder.audio_time_base_den", true);
    auto skew = parseInteger<std::int64_t>(
        node.options,
        "demux_clock_binder.first_window_maximum_skew_ns", true);
    auto regression = parseInteger<std::int64_t>(
        node.options,
        "demux_clock_binder.timestamp_regression_limit_ns", true);
    auto discontinuity = parseInteger<std::int64_t>(
        node.options,
        "demux_clock_binder.discontinuity_threshold_ns", true);
    auto generation = parseInteger<std::uint64_t>(
        node.options, "demux_clock_binder.initial_generation", true);
    auto videoIdentity = requiredNodeOption(
        &node.options, NodeName,
        "demux_clock_binder.video_source_identity");
    auto audioIdentity = requiredNodeOption(
        &node.options, NodeName,
        "demux_clock_binder.audio_source_identity");
    auto targetEpoch = parseInteger<std::int64_t>(
        node.options, "demux_clock_binder.canonical_target_epoch_ns", false);
    if (!stream || !groupText || !streamNum || !streamDen ||
        !videoNum || !videoDen || !audioNum || !audioDen || !skew ||
        !regression || !discontinuity || !generation || !videoIdentity ||
        !audioIdentity || !targetEpoch) {
        const ::media::ErrorInfo error = !stream ? stream.error()
            : !groupText ? groupText.error()
            : !streamNum ? streamNum.error()
            : !streamDen ? streamDen.error()
            : !videoNum ? videoNum.error()
            : !videoDen ? videoDen.error()
            : !audioNum ? audioNum.error()
            : !audioDen ? audioDen.error()
            : !skew ? skew.error()
            : !regression ? regression.error()
            : !discontinuity ? discontinuity.error()
            : !generation ? generation.error()
            : !videoIdentity ? videoIdentity.error()
            : !audioIdentity ? audioIdentity.error()
            : targetEpoch.error();
        return Result::failure(error);
    }
    MediaDemuxTimestampClockMapperConfig mapper{
        MediaRational{videoNum.value(), videoDen.value()},
        MediaRational{audioNum.value(), audioDen.value()},
        MediaRunningTime::fromNanoseconds(skew.value()),
        MediaRunningTime::fromNanoseconds(regression.value()),
        MediaRunningTime::fromNanoseconds(discontinuity.value()),
        generation.value(),
        std::move(videoIdentity).value(),
        std::move(audioIdentity).value(),
        MediaRunningTime::fromNanoseconds(targetEpoch.value())};
    if (!MediaDemuxTimestampClockMapper::create(mapper)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Demux binder node carries an invalid mapper product"));
    }
    const MediaRational selected{
        streamNum.value(), streamDen.value()};
    const MediaRational expected =
        stream.value() == MediaScheduledStream::Video
        ? mapper.videoTimeBase
        : mapper.audioTimeBase;
    if (selected.num != expected.num || selected.den != expected.den) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Demux binder stream time base disagrees with its mapper product"));
    }
    return Result::success(
        MediaDecodedDemuxPacketClockBinderNodePlan{
            stream.value(),
            MediaAvSyncGroupKey(std::move(groupText).value()),
            selected,
            std::move(mapper)});
}

::media::Result<MediaDemuxTimestampClockMapperConfig>
MediaDemuxPacketClockBinderNodePlanCodec::mapperConfigFromPlan(
    const MediaAvSyncPlan& plan)
{
    using Result =
        ::media::Result<MediaDemuxTimestampClockMapperConfig>;
    if (plan.sourceClockMode !=
            MediaAvSyncSourceClockMode::DemuxTimestamps ||
        !plan.demuxTimestampInput ||
        !plan.demuxTimestampInput->firstWindowMaximumSkewNs ||
        !plan.demuxTimestampInput->timestampRegressionLimitNs ||
        !plan.demuxTimestampInput->discontinuityThresholdNs ||
        !plan.demuxTimestampInput->initialGeneration ||
        !plan.startup.videoIdentity || !plan.startup.audioIdentity) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Demux binder requires the complete A/V planner authority"));
    }
    MediaDemuxTimestampClockMapperConfig config{
        plan.demuxTimestampInput->videoTimeBase,
        plan.demuxTimestampInput->audioTimeBase,
        *plan.demuxTimestampInput->firstWindowMaximumSkewNs,
        *plan.demuxTimestampInput->timestampRegressionLimitNs,
        *plan.demuxTimestampInput->discontinuityThresholdNs,
        *plan.demuxTimestampInput->initialGeneration,
        *plan.startup.videoIdentity,
        *plan.startup.audioIdentity,
        MediaRunningTime::fromNanoseconds(0)};
    auto valid = MediaDemuxTimestampClockMapper::create(config);
    return valid
        ? Result::success(std::move(config))
        : Result::failure(valid.error());
}

::media::Status
MediaDemuxPacketClockBinderNodePlanCodec::validateAgainstPlanner(
    const MediaDecodedDemuxPacketClockBinderNodePlan& decoded,
    const MediaAvSyncGroupKey& groupKey,
    const MediaAvSyncPlan& plan)
{
    auto expected = mapperConfigFromPlan(plan);
    if (!expected) return ::media::Status::failure(expected.error());
    const MediaRational selected =
        decoded.stream == MediaScheduledStream::Video
        ? expected.value().videoTimeBase
        : expected.value().audioTimeBase;
    if (!groupKey.valid() || decoded.groupKey != groupKey ||
        decoded.mapper != expected.value() ||
        decoded.streamTimeBase.num != selected.num ||
        decoded.streamTimeBase.den != selected.den) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux binder options disagree with the exact A/V planner product"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
