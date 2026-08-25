#include "internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h"

#include <limits>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::size_t> representable(
    std::uint64_t value,
    const char* fact)
{
    if (value > static_cast<std::uint64_t>(
            (std::numeric_limits<std::size_t>::max)())) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Realtime deployment ") + fact +
                " exceeds platform queue capacity"));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(value));
}

::media::Result<std::uint64_t> plannedUnits(
    std::uint64_t residenceNanoseconds,
    std::uint64_t cadenceNumerator,
    std::uint64_t cadenceDenominator,
    const char* fact)
{
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
    if (cadenceDenominator == 0 ||
        cadenceDenominator >
            (std::numeric_limits<std::uint64_t>::max)() /
                NanosecondsPerSecond) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " denominator is not representable"));
    }
    const auto denominator = cadenceDenominator * NanosecondsPerSecond;
    const auto quotient = residenceNanoseconds / denominator;
    const auto remainder = residenceNanoseconds % denominator;
    if (quotient > (std::numeric_limits<std::uint64_t>::max)() /
            cadenceNumerator ||
        remainder > (std::numeric_limits<std::uint64_t>::max)() /
            cadenceNumerator) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " is not representable"));
    }
    const auto whole = quotient * cadenceNumerator;
    const auto scaledRemainder = remainder * cadenceNumerator;
    const auto fraction = scaledRemainder / denominator +
        (scaledRemainder % denominator != 0 ? 1U : 0U);
    if (fraction >= (std::numeric_limits<std::uint64_t>::max)() - whole) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " including producer handoff is not representable"));
    }
    return ::media::Result<std::uint64_t>::success(whole + fraction + 1U);
}

} // namespace

::media::Result<MediaGraphQueueParameters> MediaRealtimeQueueCapacityPlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment,
    MediaRational outputFrameRate,
    std::optional<int> audioAccessUnitSamples,
    std::optional<int> audioSampleRate)
{
    constexpr std::size_t RetainLatestMetadataSlot = 1;
    const auto residence = deployment.encode().latency.maximumResidence;
    if (!outputFrameRate.isKnown() || outputFrameRate.num <= 0 ||
        outputFrameRate.den <= 0 || residence.nanoseconds() <= 0 ||
        audioAccessUnitSamples.has_value() != audioSampleRate.has_value() ||
        (audioAccessUnitSamples &&
         (*audioAccessUnitSamples <= 0 || *audioSampleRate <= 0))) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            ::media::ErrorInfo::notInitialized(
                "Realtime queue planning requires prepared output cadences and latency facts"));
    }
    auto videoUnits = plannedUnits(
        static_cast<std::uint64_t>(residence.nanoseconds()),
        static_cast<std::uint64_t>(outputFrameRate.num),
        static_cast<std::uint64_t>(outputFrameRate.den),
        "prepared video residence units");
    if (!videoUnits) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            videoUnits.error());
    }
    std::uint64_t audioUnits = 0;
    if (audioAccessUnitSamples) {
        auto plannedAudio = plannedUnits(
            static_cast<std::uint64_t>(residence.nanoseconds()),
            static_cast<std::uint64_t>(*audioSampleRate),
            static_cast<std::uint64_t>(*audioAccessUnitSamples),
            "prepared audio residence units");
        if (!plannedAudio) {
            return ::media::Result<MediaGraphQueueParameters>::failure(
                plannedAudio.error());
        }
        audioUnits = plannedAudio.value();
    }
    if (audioUnits > (std::numeric_limits<std::uint64_t>::max)() -
            videoUnits.value()) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared aggregate residence units are not representable"));
    }
    auto packet = representable(videoUnits.value() + audioUnits,
                                "prepared media residence units");
    auto video = representable(videoUnits.value(),
                               "prepared video residence units");
    if (!packet || !video) {
        return ::media::Result<MediaGraphQueueParameters>::failure(
            !packet ? packet.error() : video.error());
    }
    return ::media::Result<MediaGraphQueueParameters>::success(
        MediaGraphQueueParameters{RetainLatestMetadataSlot, packet.value(),
                                  video.value(), packet.value()});
}

} // namespace media::ffmpeg::graph
