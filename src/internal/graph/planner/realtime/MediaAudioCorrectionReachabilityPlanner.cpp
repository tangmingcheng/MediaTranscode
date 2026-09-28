#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"

#include <algorithm>
#include <limits>
#include <string>
#include <initializer_list>
#include <type_traits>

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

struct CorrectionTiming final {
    int outputSampleRate;
    std::int64_t worstCaseInFlightSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::size_t mailboxCapacity;
};

::media::Result<CorrectionTiming> resolveTiming(
    const MediaAudioCorrectionPlanningFacts& input)
{
    using Result = ::media::Result<CorrectionTiming>;
    const auto sumPath = [](int sampleRate, std::initializer_list<std::int64_t> path,
        std::int64_t margin, std::int64_t block, std::size_t capacity) -> Result {
        if (sampleRate <= 0 || margin <= 0 || block <= 0 || capacity == 0)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Audio correction requires positive rate, mailbox and block bounds"));
        std::int64_t worst = 0;
        for (const auto value : path) {
            auto sum = checkedAdd(worst, value, "audio in-flight");
            if (!sum) return Result::failure(sum.error());
            worst = sum.value();
        }
        return Result::success({sampleRate, worst, margin, block, capacity});
    };
    return std::visit([&](const auto& facts) -> Result {
        using Facts = std::decay_t<decltype(facts)>;
        if constexpr (std::is_same_v<Facts, MediaAudioSourceCorrectionFacts>) {
            const auto& b = facts.bounds;
            if (b.decodeQueueSamples <= 0 || b.resampleQueueSamples <= 0)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Source correction requires its actual decode and resample queue bounds"));
            return sumPath(facts.outputSampleRate,
                {b.decoderDelaySamples, b.decodeQueueSamples, b.resampleQueueSamples},
                b.mailboxDeliveryMarginSamples, b.maximumResamplerOutputBlockSamples, b.mailboxCapacity);
        } else {
            if (!facts.outputSampleRate || !facts.decoderDelaySamples ||
                !facts.encoderLookaheadSamples || !facts.decodeQueueSamples ||
                !facts.resampleQueueSamples || !facts.encodeQueueSamples ||
                !facts.schedulerQueueSamples || !facts.protocolBatchSamples ||
                !facts.mailboxDeliveryMarginSamples ||
                !facts.maximumResamplerOutputBlockSamples || !facts.mailboxCapacity)
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "A/V synchronization planning facts are incomplete"));
            return sumPath(*facts.outputSampleRate,
                {*facts.decoderDelaySamples, *facts.encoderLookaheadSamples,
                 *facts.decodeQueueSamples, *facts.resampleQueueSamples,
                 *facts.encodeQueueSamples, *facts.schedulerQueueSamples, *facts.protocolBatchSamples},
                *facts.mailboxDeliveryMarginSamples, *facts.maximumResamplerOutputBlockSamples,
                *facts.mailboxCapacity);
        }
    }, input);
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
    const MediaAudioCorrectionPlanningFacts& input)
{
    auto resolved = resolveTiming(input);
    if (!resolved) return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(resolved.error());
    const auto& facts = resolved.value();
    const auto worst = facts.worstCaseInFlightSamples;
    auto required = checkedAdd(
        worst, facts.mailboxDeliveryMarginSamples, "audio command lead");
    if (required) {
        required = checkedAdd(required.value(),
            facts.maximumResamplerOutputBlockSamples, "audio command lead");
    }
    if (!required || required.value() ==
            std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            required ? ::media::ErrorInfo::invalidArgument(
                           "audio command lead has no strict representable margin")
                     : required.error());
    }
    if (!synchronization.audioServo.maximumMeasurementGapNs ||
        !synchronization.audioServo.recoveryCorrectionLimitPpm ||
        !synchronization.audioServo.normalCorrectionLimitPpm ||
        !synchronization.audioServo.outputSampleRate ||
        *synchronization.audioServo.outputSampleRate != facts.outputSampleRate ||
        facts.maximumResamplerOutputBlockSamples <= 0) {
        return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "audio measurement correction facts are missing"));
    }
    auto measurementGapSamples = runningTimeToSamples(
        *synchronization.audioServo.maximumMeasurementGapNs,
        facts.outputSampleRate);
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
        commandLeadSamples, facts.outputSampleRate);
    auto compensationSamples = checkedAdd(
        commandLeadSamples, facts.maximumResamplerOutputBlockSamples,
        "audio compensation window");
    auto compensation = compensationSamples
        ? samplesToRunningTime(compensationSamples.value(),
                               facts.outputSampleRate)
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
    auto quantizer = MediaAudioCorrectionQuantizer::create(
        compensation.value(), commandLead.value(), facts.outputSampleRate);
    if (!quantizer) return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(quantizer.error());
    auto distance = quantizer.value().maximumCompensationDistance(
        std::max(*synchronization.audioServo.normalCorrectionLimitPpm,
                 *synchronization.audioServo.recoveryCorrectionLimitPpm));
    if (!distance) return ::media::Result<MediaAudioCorrectionReachabilityResult>::failure(distance.error());
    const auto maximumOutputBlock = std::max<std::int64_t>(
        facts.maximumResamplerOutputBlockSamples, distance.value());
    return ::media::Result<MediaAudioCorrectionReachabilityResult>::success(
        MediaAudioCorrectionReachabilityResult{
            MediaAudioCorrectionReachabilityPlan{
                facts.outputSampleRate, 0, worst,
                facts.mailboxDeliveryMarginSamples,
                facts.maximumResamplerOutputBlockSamples,
                commandLeadSamples, facts.mailboxCapacity},
            commandLead.value(), compensation.value(),
            MediaRunningTime::fromNanoseconds(frequency.value()), maximumOutputBlock});
}

} // namespace media::ffmpeg::graph
