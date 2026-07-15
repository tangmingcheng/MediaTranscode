#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"

#include <algorithm>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> checkedAdd(
    std::int64_t left, std::int64_t right, const char* owner)
{
    if (left < 0 || right < 0 ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " sample bound overflow"));
    }
    return ::media::Result<std::int64_t>::success(left + right);
}

::media::Result<std::int64_t> runningTimeToSamples(
    MediaRunningTime time, int sampleRate)
{
    constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
    if (time <= MediaRunningTime::fromNanoseconds(0) || sampleRate <= 0 ||
        time.nanoseconds() >
            (std::numeric_limits<std::int64_t>::max() -
             (NanosecondsPerSecond - 1)) / sampleRate) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio command lead cannot be converted to output samples"));
    }
    return ::media::Result<std::int64_t>::success(
        (time.nanoseconds() * sampleRate + NanosecondsPerSecond - 1) /
        NanosecondsPerSecond);
}

::media::Result<MediaRunningTime> samplesToRunningTime(
    std::int64_t samples, int sampleRate)
{
    constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
    if (samples <= 0 || sampleRate <= 0 ||
        samples > (std::numeric_limits<std::int64_t>::max() -
                   sampleRate + 1) / NanosecondsPerSecond) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio sample bound cannot be converted to running time"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            (samples * NanosecondsPerSecond + sampleRate - 1) / sampleRate));
}

} // namespace

::media::Result<MediaAudioCorrectionReachabilityResult>
MediaAudioCorrectionReachabilityPlanner::plan(
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSyncPlanningFacts& facts)
{
    if (!facts.outputSampleRate || !facts.decoderDelaySamples ||
        !facts.encoderLookaheadSamples || !facts.decodeQueueSamples ||
        !facts.resampleQueueSamples || !facts.encodeQueueSamples ||
        !facts.schedulerQueueSamples || !facts.protocolBatchSamples ||
        !facts.mailboxDeliveryMarginSamples ||
        !facts.maximumResamplerOutputBlockSamples || !facts.mailboxCapacity) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V synchronization planning facts are incomplete"));
    }
    std::int64_t worst = 0;
    for (const auto value : {
             *facts.decoderDelaySamples, *facts.encoderLookaheadSamples,
             *facts.decodeQueueSamples, *facts.resampleQueueSamples,
             *facts.encodeQueueSamples, *facts.schedulerQueueSamples,
             *facts.protocolBatchSamples}) {
        auto sum = checkedAdd(worst, value, "audio in-flight");
        if (!sum) {
            return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
                sum.error());
        }
        worst = sum.value();
    }
    auto required = checkedAdd(
        worst, *facts.mailboxDeliveryMarginSamples, "audio command lead");
    if (required) {
        required = checkedAdd(required.value(),
            *facts.maximumResamplerOutputBlockSamples, "audio command lead");
    }
    if (!required || required.value() ==
            std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            required ? ::media::ErrorInfo::invalidArgument(
                           "audio command lead has no strict representable margin")
                     : required.error());
    }
    if (!synchronization.audioServo.maximumMeasurementGapNs ||
        !synchronization.audioServo.recoveryCorrectionLimitPpm) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "audio measurement correction facts are missing"));
    }
    auto measurementGapSamples = runningTimeToSamples(
        *synchronization.audioServo.maximumMeasurementGapNs,
        *facts.outputSampleRate);
    const auto correctionPpm = static_cast<std::int64_t>(
        *synchronization.audioServo.recoveryCorrectionLimitPpm);
    if (!measurementGapSamples || correctionPpm <= 0 ||
        measurementGapSamples.value() ==
            std::numeric_limits<std::int64_t>::max() ||
        measurementGapSamples.value() >
            (std::numeric_limits<std::int64_t>::max() - 999'999) /
                correctionPpm) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            measurementGapSamples
                ? ::media::ErrorInfo::invalidArgument(
                      "audio measurement correction headroom overflow")
                : measurementGapSamples.error());
    }
    const auto correctionHeadroomSamples =
        (measurementGapSamples.value() * correctionPpm + 999'999) / 1'000'000;
    auto measurementLead = checkedAdd(
        measurementGapSamples.value(), correctionHeadroomSamples,
        "audio measurement command lead");
    if (!measurementLead || measurementLead.value() ==
            std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            measurementLead ? ::media::ErrorInfo::invalidArgument(
                                  "audio measurement command lead has no strict margin")
                            : measurementLead.error());
    }
    const auto commandLeadSamples = std::max(
        required.value() + 1, measurementLead.value() + 1);
    auto commandLead = samplesToRunningTime(
        commandLeadSamples, *facts.outputSampleRate);
    auto compensationSamples = checkedAdd(
        commandLeadSamples, *facts.maximumResamplerOutputBlockSamples,
        "audio compensation window");
    auto compensation = compensationSamples
        ? samplesToRunningTime(compensationSamples.value(),
                               *facts.outputSampleRate)
        : ::media::Result<MediaRunningTime>::failure(
              compensationSamples.error());
    auto frequency = compensation
        ? checkedAdd(compensation.value().nanoseconds(), 1'000'000'000,
                     "audio frequency filter")
        : ::media::Result<std::int64_t>::failure(compensation.error());
    if (!commandLead || !compensation || !frequency ||
        frequency.value() > 60'000'000'000) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "bounded audio queues exceed the synchronization policy duration"));
    }
    return ::media::Result<MediaAudioCorrectionReachabilityResult>::success(
        MediaAudioCorrectionReachabilityResult{
            MediaAudioCorrectionReachabilityPlan{
                *facts.outputSampleRate, 0, worst,
                *facts.protocolBatchSamples,
                *facts.mailboxDeliveryMarginSamples,
                *facts.maximumResamplerOutputBlockSamples,
                commandLeadSamples, *facts.mailboxCapacity},
            commandLead.value(), compensation.value(),
            MediaRunningTime::fromNanoseconds(frequency.value())});
}

} // namespace media::ffmpeg::graph
