#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t BitsPerByte = 8;
constexpr std::uint64_t Kilo = 1000;

::media::Result<std::uint64_t> checkedKbitsToBytes(
    std::uint64_t kbits, const char* fact)
{
    if (kbits > (std::numeric_limits<std::uint64_t>::max)() / Kilo) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " is not representable"));
    }
    const auto bits = kbits * Kilo;
    return ::media::Result<std::uint64_t>::success(
        bits / BitsPerByte + (bits % BitsPerByte != 0 ? 1U : 0U));
}

} // namespace

::media::Result<MediaRealtimeMediaCapacityPlan>
MediaRealtimeMediaCapacityPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    if (!request.deployment ||
        !request.parameters.execution.streamSet ||
        request.parameters.queues.packet == 0) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "Realtime media capacity requires deployment, stream-set, and planned queue facts"));
    }
    const auto& video = request.parameters.video;
    if (!video.bitrateKbps || *video.bitrateKbps <= 0 ||
        !video.bufferSizeKbits || *video.bufferSizeKbits <= 0 ||
        !video.frameRate.complete() || !video.frameRate.numerator ||
        !video.frameRate.denominator || *video.frameRate.numerator <= 0 ||
        *video.frameRate.denominator <= 0) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "Realtime media capacity requires prepared video emission, VBV, and cadence facts"));
    }
    const auto& deployment = request.deployment->encode();
    const auto maximumVideoKbps = static_cast<std::uint64_t>(
        video.maxBitrateKbps.value_or(*video.bitrateKbps));
    auto videoBytesPerSecond = checkedKbitsToBytes(
        maximumVideoKbps, "video peak byte rate");
    auto videoUnitBytes = checkedKbitsToBytes(
        static_cast<std::uint64_t>(*video.bufferSizeKbits),
        "video VBV bytes");
    auto videoResidenceBytes = videoBytesPerSecond
        ? MediaRealtimePlanningArithmetic::bytesForResidence(
              videoBytesPerSecond.value(),
              deployment.latency.maximumResidence,
              "video residence bytes")
        : videoBytesPerSecond;
    auto plannedVideoBytes = videoResidenceBytes && videoUnitBytes
        ? MediaRealtimePlanningArithmetic::add(
              videoResidenceBytes.value(), videoUnitBytes.value(),
              "video residence and VBV bytes")
        : ::media::Result<std::uint64_t>::failure(
              !videoResidenceBytes ? videoResidenceBytes.error()
                                   : videoUnitBytes.error());
    if (!plannedVideoBytes) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            plannedVideoBytes.error());
    }
    const auto videoBytes = plannedVideoBytes.value();
    std::optional<std::size_t> audioUnits;
    std::optional<std::uint64_t> audioUnitBytes;
    std::optional<std::uint64_t> audioBytes;
    if (*request.parameters.execution.streamSet ==
        MediaTranscodeStreamSet::AudioVideo) {
        const auto& audio = request.parameters.audio;
        if (!audio.bitrateKbps || *audio.bitrateKbps <= 0 ||
            !audio.sampleRate || *audio.sampleRate <= 0) {
            return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Realtime media capacity requires prepared audio emission facts"));
        }
        const auto maximumAudioKbps = static_cast<std::uint64_t>(
            audio.maxBitrateKbps.value_or(*audio.bitrateKbps));
        auto audioBytesPerSecond = checkedKbitsToBytes(
            maximumAudioKbps, "audio peak byte rate");
        if (!audioBytesPerSecond) {
            return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
                audioBytesPerSecond.error());
        }
        const auto plannedAudioUnits = request.parameters.queues.packet >
                request.parameters.queues.frame
            ? request.parameters.queues.packet - request.parameters.queues.frame
            : std::size_t{1};
        audioUnits = plannedAudioUnits;
        if (audio.bufferSizeKbits && *audio.bufferSizeKbits > 0) {
            auto plannedAudioUnit = checkedKbitsToBytes(
                static_cast<std::uint64_t>(*audio.bufferSizeKbits),
                "audio buffer bytes");
            if (!plannedAudioUnit) {
                return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
                    plannedAudioUnit.error());
            }
            audioUnitBytes = plannedAudioUnit.value();
        } else {
            audioUnitBytes = audioBytesPerSecond.value() / plannedAudioUnits +
                (audioBytesPerSecond.value() % plannedAudioUnits != 0 ? 1U : 0U);
        }
        auto audioResidenceBytes =
            MediaRealtimePlanningArithmetic::bytesForResidence(
            audioBytesPerSecond.value(), deployment.latency.maximumResidence,
            "audio residence bytes");
        auto plannedAudioBytes = audioResidenceBytes
            ? MediaRealtimePlanningArithmetic::add(
                  audioResidenceBytes.value(), *audioUnitBytes,
                  "audio residence and unit bytes")
            : audioResidenceBytes;
        if (!plannedAudioBytes) {
            return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
                plannedAudioBytes.error());
        }
        audioBytes = plannedAudioBytes.value();
    }
    auto totalBytes = MediaRealtimePlanningArithmetic::add(
        videoBytes, audioBytes.value_or(0), "aggregate graph media bytes");
    if (!totalBytes || videoBytes == 0 || videoUnitBytes.value() == 0 ||
        totalBytes.value() > deployment.resources.maximumGraphMemoryBytes ||
        totalBytes.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Realtime graph memory budget cannot admit prepared media emission within the latency bound"));
    }
    MediaRealtimeMediaCapacityPlan product{
        request.parameters.queues.frame,
        videoUnitBytes.value(),
        videoBytes,
        audioUnits,
        audioUnitBytes,
        audioBytes,
        deployment.latency.maximumResidence};
    return ::media::Result<MediaRealtimeMediaCapacityPlan>::success(
        std::move(product));
}

} // namespace media::ffmpeg::graph
