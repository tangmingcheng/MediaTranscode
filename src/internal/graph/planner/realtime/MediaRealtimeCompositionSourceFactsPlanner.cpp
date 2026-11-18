#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceFactsPlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpVideoSignalingResolver.h"
#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"
#include "internal/graph/protocol/codec/MediaH264SpsCodedSizeParser.h"
#include "internal/graph/protocol/codec/MediaHevcSpsCodedSizeParser.h"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeCompositionSourceFacts>
MediaRealtimeCompositionSourceFactsPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request, std::size_t index,
    const MediaRealtimeCompositionInputPlan& input,
    const MediaRtpSourceObservation& observed)
{
    using Result = ::media::Result<MediaRealtimeCompositionSourceFacts>;
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&observed.timing);
    if (observed.timing.valueless_by_exception() ||
        (cadence && (cadence->rate.num <= 0 || cadence->rate.den <= 0)))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Composition source timing contains invalid observed cadence"));
    auto source = request;
    source.input = input.input;
    source.parameters.execution.streamSet = input.members;
    source.composition.reset();
    source.mediaId += ".source." + std::to_string(index);
    auto signaling = MediaRealtimeRtpVideoSignalingResolver::resolve(source.input.videoRtp,
        source.input.videoRtp.fmtp ? nullptr : &observed.signaling);
    if (!signaling) return Result::failure(signaling.error());
    source.input.videoRtp.fmtp = signaling.value().fmtp;
    auto clock = MediaAvSyncPlanner::planRtpInputClock(source,
        MediaAvSourceLifecycleMode::PreserveActivatedOutput);
    if (!clock) return Result::failure(clock.error());
    MediaAvSyncPlan inputClock;
    inputClock.members = input.members;
    inputClock.sourceClockMode = MediaAvSyncSourceClockMode::RtpSenderReports;
    inputClock.rtpInput = std::move(clock).value();
    auto raw = MediaRealtimeInputPlanner::planRawRtp(source, &inputClock);
    if (!raw) return Result::failure(raw.error());
    raw.value().video.width = signaling.value().codedSize.width;
    raw.value().video.height = signaling.value().codedSize.height;
    if (cadence) raw.value().video.frameRate = cadence->rate;
    raw.value().video.sampleAspectRatio = signaling.value().sampleAspectRatio;
    std::optional<MediaVideoColorRangeFact> color;
    auto parsed = std::visit([&](const auto& facts) {
        using Facts = std::decay_t<decltype(facts)>;
        if constexpr (std::is_same_v<Facts, MediaH264SignalingFacts>)
            return MediaH264SpsCodedSizeParser::parse(facts.sps, &color);
        else return MediaHevcSpsCodedSizeParser::parse(facts.sps, &color);
    }, observed.signaling.facts);
    if (!parsed) return Result::failure(parsed.error());
    if (!color) return Result::failure(::media::ErrorInfo::unsupported(
        "Composition source SPS has no authoritative color-range fact"));
    raw.value().video.sourceColorRange = color->range;
    return Result::success({std::move(source), std::move(raw).value(), *color, observed.timing});
}

} // namespace media::ffmpeg::graph
